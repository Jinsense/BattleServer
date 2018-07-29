#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <windows.h>

#include "LanMasterClient.h"
#include "GameServer.h"

using namespace std;

CLanMasterClient::CLanMasterClient() :
	m_iRecvPacketTPS(NULL),
	m_iSendPacketTPS(NULL)
{
	m_Session = new LANCLIENTSESSION;

	m_Release = false;
	m_Reconnect = false;
	setlocale(LC_ALL, "Korean");

	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
}

CLanMasterClient::~CLanMasterClient()
{
	delete m_Session;
}

void CLanMasterClient::Constructor(CGameServer *pGameServer)
{
	_pGameServer = pGameServer;
	return;
}

void CLanMasterClient::OnEnterJoinServer()
{
	//	�������� ���� ���� ��
	CPacket *pPacket = CPacket::Alloc();

	WORD Type = en_PACKET_BAT_MAS_REQ_SERVER_ON;
	WORD BattleServerPort = Config.BATTLE_BIND_PORT;
	char MasterToken[32] = { 0, };
	memcpy_s(&MasterToken, sizeof(MasterToken), &Config.MASTERTOKEN, sizeof(MasterToken));

	*pPacket << Type;
	pPacket->PushData((char*)&Config.BATTLE_BIND_IP, sizeof(Config.BATTLE_BIND_IP));
	*pPacket << BattleServerPort;
	pPacket->PushData((char*)&_pGameServer->_CurConnectToken, sizeof(_pGameServer->_CurConnectToken));
	pPacket->PushData((char*)&MasterToken, sizeof(MasterToken));
	pPacket->PushData((char*)&Config.CHAT_BIND_IP, sizeof(Config.CHAT_BIND_IP));
	*pPacket << Config.CHAT_BIND_PORT;

	SendPacket(pPacket);
	pPacket->Free();
	return;
}

void CLanMasterClient::OnLeaveServer()
{
	//	�������� ������ �������� ��
	m_Session->bConnect = false;
	m_Reconnect = true;
	m_Release = true;
	return;
}

void CLanMasterClient::OnLanRecv(CPacket *pPacket)
{
	//-------------------------------------------------------------
	//	��Ŷ ó�� - ������ ó��
	//-------------------------------------------------------------
	WORD Type;
	*pPacket >> Type;
	//-------------------------------------------------------------
	//	��Ŷ ó�� - ��Ʋ ���� ���� ���� Ȯ��
	//	Type : en_PACKET_BAT_MAS_RES_SERVER_ON
	//	int	 : BattleServerNo
	//-------------------------------------------------------------
	if (Type == en_PACKET_BAT_MAS_RES_SERVER_ON)
	{
		ResBattleOn(pPacket);
		return;
	}
	//-------------------------------------------------------------
	//	��Ŷ ó�� - ��Ʋ ���� ������ū ����� ����
	//	Type : en_PACKET_BAT_MAS_RES_CONNECT_TOKEN
	//-------------------------------------------------------------
	else if (Type == en_PACKET_BAT_MAS_RES_CONNECT_TOKEN)
	{
		ResBattleConnectToken(pPacket);
		return;
	}
	//-------------------------------------------------------------
	//	��Ŷ ó�� - ��Ʋ ���� �ű� ���� ���� ����
	//	Type : en_PACKET_BAT_MAS_RES_CREATED_ROOM
	//-------------------------------------------------------------
	else if (Type == en_PACKET_BAT_MAS_RES_CREATED_ROOM)
	{
		ResBattleCreateRoom(pPacket);
		return;
	}
	//-------------------------------------------------------------
	//	��Ŷ ó�� - ��Ʋ ���� ���� ���� Ȯ��
	//	Type : en_PACKET_BAT_MAS_RES_CLOSED_ROOM
	//-------------------------------------------------------------
	else if (Type == en_PACKET_BAT_MAS_RES_CLOSED_ROOM)
	{
		ResBattleClosedRoom(pPacket);
		return;
	}
	//-------------------------------------------------------------
	//	��Ŷ ó�� - ��Ʋ ���� ���� ���� ���� Ȯ��
	//	Type : en_PACKET_BAT_MAS_RES_LEFT_USER
	//-------------------------------------------------------------
	else if (Type == en_PACKET_BAT_MAS_RES_LEFT_USER)
	{
		ResBattleLeftUser(pPacket);
		return;
	}

	return;
}

void CLanMasterClient::OnLanSend(int SendSize)
{
	//	��Ŷ �۽� �Ϸ� ��

	return;
}

void CLanMasterClient::OnWorkerThreadBegin()
{

}

void CLanMasterClient::OnWorkerThreadEnd()
{

}

void CLanMasterClient::OnError(int ErrorCode, WCHAR *pMsg)
{

}

bool CLanMasterClient::Connect(WCHAR * ServerIP, int Port, bool bNoDelay, int MaxWorkerThread)
{
	wprintf(L"[Client :: ClientInit]	Start\n");

	m_Session->RecvQ.Clear();
	m_Session->PacketQ.Clear();
	m_Session->SendFlag = false;
	m_Release = false;

	for (auto i = 0; i < MaxWorkerThread; i++)
	{
		m_hWorker_Thread[i] = (HANDLE)_beginthreadex(NULL, 0, &WorkerThread, (LPVOID)this, 0, NULL);
	}

	WSADATA wsaData;
	int retval = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != retval)
	{
		wprintf(L"[Client :: Connect]	WSAStartup Error\n");
		//	�α�
		g_CrashDump->Crash();
		return false;
	}

	m_Session->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_Session->sock == INVALID_SOCKET)
	{
		wprintf(L"[Client :: Connect]	WSASocket Error\n");
		//	�α�
		g_CrashDump->Crash();
		return false;
	}

	struct sockaddr_in client_addr;
	ZeroMemory(&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	InetPton(AF_INET, ServerIP, &client_addr.sin_addr.s_addr);

	client_addr.sin_port = htons(Port);
	setsockopt(m_Session->sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&bNoDelay, sizeof(bNoDelay));

	while (1)
	{
		retval = connect(m_Session->sock, (SOCKADDR*)&client_addr, sizeof(client_addr));
		if (retval == SOCKET_ERROR)
		{
			wprintf(L"[Client :: Connect]		Login_LanServer Not Connect\n");
			Sleep(1000);
			continue;
		}
		break;
	}
	InterlockedIncrement(&m_Session->IO_Count);
	CreateIoCompletionPort((HANDLE)m_Session->sock, m_hIOCP, (ULONG_PTR)this, 0);

	OnEnterJoinServer();
	wprintf(L"[Client :: Connect]		Complete\n");
	StartRecvPost();
	return true;
}

bool CLanMasterClient::Disconnect()
{
	closesocket(m_Session->sock);
	m_Session->sock = INVALID_SOCKET;

	while (0 < m_Session->SendQ.GetUseCount())
	{
		CPacket * pPacket;
		m_Session->SendQ.Dequeue(pPacket);
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket->Free();
	}

	while (0 < m_Session->PacketQ.GetUseSize())
	{
		CPacket * pPacket;
		m_Session->PacketQ.Peek((char*)&pPacket, sizeof(CPacket*));
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket->Free();
		m_Session->PacketQ.Dequeue(sizeof(CPacket*));
	}
	OnLeaveServer();
	//	m_Session->bConnect = false;

	WaitForMultipleObjects(LANCLIENT_WORKERTHREAD, m_hWorker_Thread, false, INFINITE);

	for (auto iCnt = 0; iCnt < LANCLIENT_WORKERTHREAD; iCnt++)
	{
		CloseHandle(m_hWorker_Thread[iCnt]);
		m_hWorker_Thread[iCnt] = INVALID_HANDLE_VALUE;
	}

	WSACleanup();
	return true;
}

bool CLanMasterClient::IsConnect()
{
	return m_Session->bConnect;
}

bool CLanMasterClient::SendPacket(CPacket *pPacket)
{
	m_iSendPacketTPS++;
	pPacket->AddRef();
	pPacket->SetHeader_CustomShort(pPacket->GetDataSize());
	m_Session->SendQ.Enqueue(pPacket);
	SendPost();

	return true;
}

void CLanMasterClient::WorkerThread_Update()
{
	DWORD retval;

	while (!m_Release)
	{
		//	�ʱ�ȭ �ʼ�
		OVERLAPPED * pOver = NULL;
		LANCLIENTSESSION * pSession = NULL;
		DWORD Trans = 0;

		retval = GetQueuedCompletionStatus(m_hIOCP, &Trans, (PULONG_PTR)&pSession, (LPWSAOVERLAPPED*)&pOver, INFINITE);
		//		OnWorkerThreadBegin();

		if (nullptr == pOver)
		{
			if (FALSE == retval)
			{
				int LastError = GetLastError();
				if (WSA_OPERATION_ABORTED == LastError)
				{

				}
				//	IOCP ��ü ����
				g_CrashDump->Crash();
			}
			else if (0 == Trans)
			{
				PostQueuedCompletionStatus(m_hIOCP, 0, 0, 0);
			}
			else
			{
				//	���� �������� �߻��Ҽ� ���� ��Ȳ
				g_CrashDump->Crash();
			}
			break;
		}

		if (0 == Trans)
		{
			shutdown(m_Session->sock, SD_BOTH);
			//			Disconnect();
		}
		else if (pOver == &m_Session->RecvOver)
		{
			CompleteRecv(Trans);
		}
		else if (pOver == &m_Session->SendOver)
		{
			CompleteSend(Trans);
		}

		if (0 >= (retval = InterlockedDecrement(&m_Session->IO_Count)))
		{
			if (0 == retval)
				Disconnect();
			else if (0 > retval)
				g_CrashDump->Crash();
		}
		//		OnWorkerThreadEnd();
	}

}

void CLanMasterClient::CompleteRecv(DWORD dwTransfered)
{
	m_Session->RecvQ.Enqueue(dwTransfered);
	WORD _wPayloadSize = 0;

	while (LANCLIENT_HEADERSIZE == m_Session->RecvQ.Peek((char*)&_wPayloadSize, LANCLIENT_HEADERSIZE))
	{
		if (m_Session->RecvQ.GetUseSize() < LANCLIENT_HEADERSIZE + _wPayloadSize)
			break;

		m_Session->RecvQ.Dequeue(LANCLIENT_HEADERSIZE);
		CPacket *_pPacket = CPacket::Alloc();
		if (_pPacket->GetFreeSize() < _wPayloadSize)
		{
			_pPacket->Free();
			Disconnect();
			return;
		}
		m_Session->RecvQ.Dequeue(_pPacket->GetWritePtr(), _wPayloadSize);
		_pPacket->PushData(_wPayloadSize + sizeof(CPacket::st_PACKET_HEADER));
		_pPacket->PopData(sizeof(CPacket::st_PACKET_HEADER));

		m_iRecvPacketTPS++;
		OnLanRecv(_pPacket);
		_pPacket->Free();
	}
	RecvPost();
}

void CLanMasterClient::CompleteSend(DWORD dwTransfered)
{
	CPacket * pPacket[LANCLIENT_WSABUFNUM];
	int Num = m_Session->Send_Count;

	m_Session->PacketQ.Peek((char*)&pPacket, sizeof(CPacket*) * Num);
	for (auto i = 0; i < Num; i++)
	{
		if (pPacket == nullptr)
			g_CrashDump->Crash();
		pPacket[i]->Free();
		m_Session->PacketQ.Dequeue(sizeof(CPacket*));
	}
	m_Session->Send_Count -= Num;

	InterlockedExchange(&m_Session->SendFlag, false);

	SendPost();
}

void CLanMasterClient::StartRecvPost()
{
	DWORD flags = 0;
	ZeroMemory(&m_Session->RecvOver, sizeof(m_Session->RecvOver));

	WSABUF wsaBuf[2];
	DWORD freeSize = m_Session->RecvQ.GetFreeSize();
	DWORD notBrokenPushSize = m_Session->RecvQ.GetNotBrokenPushSize();
	if (0 == freeSize && 0 == notBrokenPushSize)
	{
		//	�α�
		//	RecvQ�� �� ���� �������� ������ ����
		g_CrashDump->Crash();
		return;
	}

	int numOfBuf = (notBrokenPushSize < freeSize) ? 2 : 1;

	wsaBuf[0].buf = m_Session->RecvQ.GetWriteBufferPtr();		//	Dequeue�� rear�� �ǵ帮�� �����Ƿ� ����
	wsaBuf[0].len = notBrokenPushSize;

	if (2 == numOfBuf)
	{
		wsaBuf[1].buf = m_Session->RecvQ.GetBufferPtr();
		wsaBuf[1].len = freeSize - notBrokenPushSize;
	}

	if (SOCKET_ERROR == WSARecv(m_Session->sock, wsaBuf, numOfBuf, NULL, &flags, &m_Session->RecvOver, NULL))
	{
		int lastError = WSAGetLastError();
		if (ERROR_IO_PENDING != lastError)
		{
			if (0 != InterlockedDecrement(&m_Session->IO_Count))
			{
				_pGameServer->_pLog->Log(L"Error", LOG_SYSTEM, L"Recv SocketError - Code %d", lastError);
				shutdown(m_Session->sock, SD_BOTH);
			}
			//			Disconnect();
		}
	}
	return;
}

void CLanMasterClient::RecvPost()
{
	int Count = InterlockedIncrement(&m_Session->IO_Count);
	if (1 == Count)
	{
		InterlockedDecrement(&m_Session->IO_Count);
		return;
	}

	DWORD flags = 0;
	ZeroMemory(&m_Session->RecvOver, sizeof(m_Session->RecvOver));

	WSABUF wsaBuf[2];
	DWORD freeSize = m_Session->RecvQ.GetFreeSize();
	DWORD notBrokenPushSize = m_Session->RecvQ.GetNotBrokenPushSize();
	if (0 == freeSize && 0 == notBrokenPushSize)
	{
		//	�α�
		//	RecvQ�� �� ���� �������� ������ ����
		g_CrashDump->Crash();
		return;
	}

	int numOfBuf = (notBrokenPushSize < freeSize) ? 2 : 1;

	wsaBuf[0].buf = m_Session->RecvQ.GetWriteBufferPtr();		//	Dequeue�� rear�� �ǵ帮�� �����Ƿ� ����
	wsaBuf[0].len = notBrokenPushSize;

	if (2 == numOfBuf)
	{
		wsaBuf[1].buf = m_Session->RecvQ.GetBufferPtr();
		wsaBuf[1].len = freeSize - notBrokenPushSize;
	}

	if (SOCKET_ERROR == WSARecv(m_Session->sock, wsaBuf, numOfBuf, NULL, &flags, &m_Session->RecvOver, NULL))
	{
		int lastError = WSAGetLastError();
		if (ERROR_IO_PENDING != lastError)
		{
			if (0 != InterlockedDecrement(&m_Session->IO_Count))
			{
				_pGameServer->_pLog->Log(L"shutdown", LOG_SYSTEM, L"LanClient Recv SocketError - Code %d", lastError);
				shutdown(m_Session->sock, SD_BOTH);
			}
			//			Disconnect();
		}

	}
	return;
}

void CLanMasterClient::SendPost()
{
	do
	{
		if (true == InterlockedCompareExchange(&m_Session->SendFlag, true, false))
			return;

		if (0 == m_Session->SendQ.GetUseCount())
		{
			InterlockedExchange(&m_Session->SendFlag, false);
			return;
		}

		WSABUF wsaBuf[LANCLIENT_WSABUFNUM];
		CPacket *pPacket;
		long BufNum = 0;
		int iUseSize = (m_Session->SendQ.GetUseCount());
		if (iUseSize > LANCLIENT_WSABUFNUM)
		{
			//	SendQ�� ��Ŷ�� 100�� �̻�������, WSABUF�� 100���� �ִ´�.
			BufNum = LANCLIENT_WSABUFNUM;
			m_Session->Send_Count = LANCLIENT_WSABUFNUM;
			//	pSession->SendQ.Peek((char*)&pPacket, sizeof(CPacket*) * MAX_WSABUF_NUMBER);
			for (auto i = 0; i < LANCLIENT_WSABUFNUM; i++)
			{
				m_Session->SendQ.Dequeue(pPacket);
				m_Session->PacketQ.Enqueue((char*)&pPacket, sizeof(CPacket*));
				wsaBuf[i].buf = pPacket->GetReadPtr();
				wsaBuf[i].len = pPacket->GetPacketSize_CustomHeader(LANCLIENT_HEADERSIZE);
			}
		}
		else
		{
			//	SendQ�� ��Ŷ�� 100�� ������ �� ���� ��Ŷ�� ����ؼ� �ִ´�
			BufNum = iUseSize;
			m_Session->Send_Count = iUseSize;
			//			pSession->SendQ.Peek((char*)&pPacket, sizeof(CPacket*) * iUseSize);
			for (auto i = 0; i < iUseSize; i++)
			{
				m_Session->SendQ.Dequeue(pPacket);
				m_Session->PacketQ.Enqueue((char*)&pPacket, sizeof(CPacket*));
				wsaBuf[i].buf = pPacket->GetReadPtr();
				wsaBuf[i].len = pPacket->GetPacketSize_CustomHeader(LANCLIENT_HEADERSIZE);
			}
		}

		if (1 == InterlockedIncrement(&m_Session->IO_Count))
		{
			InterlockedDecrement(&m_Session->IO_Count);
			InterlockedExchange(&m_Session->SendFlag, false);
			return;
		}

		ZeroMemory(&m_Session->SendOver, sizeof(m_Session->SendOver));
		if (SOCKET_ERROR == WSASend(m_Session->sock, wsaBuf, BufNum, NULL, 0, &m_Session->SendOver, NULL))
		{
			int lastError = WSAGetLastError();
			if (ERROR_IO_PENDING != lastError)
			{
				if (0 != InterlockedDecrement(&m_Session->IO_Count))
				{
					_pGameServer->_pLog->Log(L"shutdown", LOG_SYSTEM, L"LanClient Send SocketError - Code %d", lastError);
					shutdown(m_Session->sock, SD_BOTH);
				}
				//				Disconnect();
				return;
			}
		}
	} while (0 != m_Session->SendQ.GetUseCount());
}

void CLanMasterClient::ResBattleOn(CPacket * pPacket)
{
	*pPacket >> _pGameServer->_BattleServerNo;
	//	������ ���� ����� ���� �翬������ Ȯ��
	if (true == m_Reconnect)
	{
		//	���� ������ ���� ����Ʈ�� �ٽ� ������ �ش�.
		//	�̶� ������ �ִ� ���� MaxUser ��ġ�� ����ؼ� �������ش�.
		AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
		for (auto i = _pGameServer->_WaitRoomMap.begin(); i != _pGameServer->_WaitRoomMap.end(); i++)
		{
			if (false == (*i).second->RoomReady && 0 < (*i).second->MaxUser - (*i).second->CurUser)
			{
				CPacket *pPacket = CPacket::Alloc();
				WORD Type = en_PACKET_BAT_MAS_REQ_CREATED_ROOM;
				*pPacket << Type << _pGameServer->_BattleServerNo << (*i).second->RoomNo << ((*i).second->MaxUser - (*i).second->CurUser);
				pPacket->PushData((char*)&(*i).second->Entertoken, sizeof((*i).second->Entertoken));
				*pPacket << _pGameServer->_Sequence;
				SendPacket(pPacket);
				pPacket->Free();
			}
		}
		ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	}
	m_Session->bConnect = true;
	return;
}

void CLanMasterClient::ResBattleConnectToken(CPacket * pPacket)
{

	return;
}

void CLanMasterClient::ResBattleCreateRoom(CPacket * pPacket)
{
	int RoomNo = NULL;
	*pPacket >> RoomNo;
	//	Map�� �ִ� RoomNo���� �˻� - ���� ��� �α� �� �� ���� ��Ŷ ����
	//	Ȥ�� �ű� ���� ����Ʈ ������ �� �ش� ����Ʈ���� �� ���� ( ���� ��� �α� �� ���� or ũ���� )
	/*std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	iter = _pGameServer->_WaitRoomMap.find(RoomNo);
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	if (iter == _pGameServer->_WaitRoomMap.end())
	{
	_pGameServer->_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"CreateRoomRes - RoomNo is Not Find [RoomNo : %d]"), RoomNo);
	CPacket * CloseRoomPacket = CPacket::Alloc();
	WORD Type = en_PACKET_BAT_MAS_REQ_CLOSED_ROOM;
	*CloseRoomPacket << Type << RoomNo << _pGameServer->_Sequence;
	_pGameServer->_pMaster->SendPacket(CloseRoomPacket);
	CloseRoomPacket->Free();
	}*/
	return;
}

void CLanMasterClient::ResBattleClosedRoom(CPacket * pPacket)
{
	//	��Ʋ ���� ���� ���� ����Ʈ ���� �� �ش� ����Ʈ���� ���� 
	//	���� ��� �α� �� ���� Ȥ�� ũ����
	int RoomNo = NULL;
	*pPacket >> RoomNo;
	std::list<int>::iterator iter;
	AcquireSRWLockExclusive(&_pGameServer->_ClosedRoom_lock);
	for (iter = _pGameServer->_ClosedRoomlist.begin(); iter != _pGameServer->_ClosedRoomlist.end();)
	{
		if ((*iter) == RoomNo)
		{
			iter = _pGameServer->_ClosedRoomlist.erase(iter);
			break;
		}
		else if (iter == _pGameServer->_ClosedRoomlist.end())
		{
			_pGameServer->_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"ClosedRoomRes - RoomNo is Not Find [RoomNo : %d]"), RoomNo);
			iter++;
			break;
		}
		else
			iter++;
	}
	ReleaseSRWLockExclusive(&_pGameServer->_ClosedRoom_lock);
	return;
}

void CLanMasterClient::ResBattleLeftUser(CPacket * pPacket)
{

	return;
}
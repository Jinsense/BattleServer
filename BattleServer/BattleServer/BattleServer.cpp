#include <WinSock2.h>
#include <windows.h>
#include <wchar.h>
#include <Ws2tcpip.h>
#include <Mstcpip.h>
#include <process.h>
#include <time.h>
#include <chrono>
#include <iostream>

using namespace std;

#include "BattleServer.h"

CBattleServer::CBattleServer()
{
}

CBattleServer::CBattleServer(int iMaxSession, int iSend, int iAuth, int iGame) : _iMaxSession(iMaxSession), _AccpetSocketQueue(50000), _iSendThread(iSend), _iAuthThread(iAuth), _iGameThread(iGame)
{
	timeBeginPeriod(1);

	_bShutdown = false;
	_bShutdownListen = false;
	_ListenSocket = INVALID_SOCKET;
	_bEnableNagle = false;
	_iWorkerThread = WORKER_THREAD_MAX;

	ZeroMemory(_szListenIP, sizeof(_szListenIP));
	_iListenPort = NULL;
	_byCode = NULL;
	_iClientIDCnt = 0;
	_hAcceptThread = NULL;
	_hAuthThread = NULL;
	for (auto iCnt = 0; iCnt < WORKER_THREAD_MAX; iCnt++)
		_hIOCPWorkerThread[iCnt] = NULL;
	_hIOCP = NULL;
	_hSendThread = NULL;
	_pSessionArray = new CNetSession*[_iMaxSession];
	_pMemoryPool_ConnectInfo = new CMemoryPool<CLIENT_CONNECT_INFO>();
	InitializeSRWLock(&_Srwlock);
	InitializeSRWLock(&_AccountNoMap_srwlock);
	CPacket::MemoryPoolInit();
	_pLog = _pLog->GetInstance();

	_Monitor_AcceptTotal = NULL;
	_Monitor_AcceptSocket = NULL;
	_Monitor_SessionAllMode = NULL;
	_Monitor_SessionAuthMode = NULL;
	_Monitor_SessionGameMode = NULL;
	_Monitor_Counter_AuthUpdate = NULL;
	_Monitor_Counter_GameUpdate = NULL;
	_Monitor_Counter_Recv = NULL;
	_Monitor_Counter_Send = NULL;
	_Monitor_Counter_PacketSend = NULL;

	_Monitor_Counter_RecvAvr = NULL;
	_Monitor_Counter_SendAvr = NULL;
	_Monitor_Counter_AcceptThreadAvr = NULL;
	_Monitor_Counter_SendThreadAvr = NULL;
	_Monitor_Counter_AuthThreadAvr = NULL;
	_Monitor_Counter_GameThreadAvr = NULL;

	_AccpetSocketQueue.Clear();
}

CBattleServer::~CBattleServer()
{
	timeEndPeriod(1);
	delete[] _pSessionArray;
}

bool CBattleServer::Start(WCHAR *szListenIP, int iPort, int iWorkerThread, bool bEnableNagle, BYTE byPacketCode, BYTE byPacketKey1, BYTE byPacketKey2)
{
	int iRetval = 0;
	setlocale(LC_ALL, "Korean");
	
	CPacket::Init(byPacketCode, byPacketKey1, byPacketKey2);
	_byCode = byPacketCode;

	for (int i = 0; i < _iMaxSession; i++)
		_BlankSessionStack.Push(i);

	WSADATA Data;
	if (0 != WSAStartup(MAKEWORD(2, 2), &Data))
		return false;
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	if (NULL == _hIOCP)
		return false;
	_ListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == _ListenSocket)
		return false;

	struct sockaddr_in Server_addr;
	ZeroMemory(&Server_addr, sizeof(Server_addr));
	Server_addr.sin_family = AF_INET;
	InetPton(AF_INET, (PCWSTR)szListenIP, &Server_addr.sin_addr);
	Server_addr.sin_port = htons(iPort);
	if (0 != setsockopt(_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&bEnableNagle, sizeof(bEnableNagle)))
	{
		int LastError = WSAGetLastError();
		wprintf(L"[Sock opt Error : %d\n", LastError);
		return false;
	}
	
	tcp_keepalive tcpkl;
	DWORD dwResult;
	tcpkl.onoff = 1;				// KEEPALIVE ON
	tcpkl.keepalivetime = 10000;	// 10초 마다 KEEPALIVE 신호를 보내겠다. (윈도우 기본은 2시간)
	tcpkl.keepaliveinterval = 1000;	// keepalive 신호를 보내고 응답이 없으면 1초마다 재 전송하겠다. (ms tcp 는 10회 재시도 한다)
	WSAIoctl(_ListenSocket, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive), 0, 0, &dwResult, NULL, NULL);
	
	iRetval = ::bind(_ListenSocket, (sockaddr*)&Server_addr, sizeof(Server_addr));
	if (SOCKET_ERROR == iRetval)
	{
		int LastError = WSAGetLastError();
		wprintf(L"[Sock opt Error : %d\n", LastError);
		return false;
	}

	iRetval = listen(_ListenSocket, SOMAXCONN);
	if (SOCKET_ERROR == iRetval)
	{
		int LastError = WSAGetLastError();
		wprintf(L"[Sock opt Error : %d\n", LastError);
		return false;
	}

	CreateThread();
	return true;
}

bool CBattleServer::Stop()
{

	return true;
}

void CBattleServer::SetSessionArray(int iArrayIndex, CNetSession *pSession)
{
	_pSessionArray[iArrayIndex] = pSession;
	_pSessionArray[iArrayIndex]->Set(this);
	return;
}

bool CBattleServer::CreateThread()
{
	_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, &AcceptThread, (LPVOID)this, 0, NULL);
	for(int i = 0; i < WORKER_THREAD_MAX; i++)
		_hIOCPWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, &IOCPWorkerThread, (LPVOID)this, 0, NULL);
	_hSendThread = (HANDLE)_beginthreadex(NULL, 0, &SendThread, (LPVOID)this, 0, NULL);
	_hAuthThread = (HANDLE)_beginthreadex(NULL, 0, &AuthThread, (LPVOID)this, 0, NULL);
	_hGameUpdateThread = (HANDLE)_beginthreadex(NULL, 0, &GameUpdateThread, (LPVOID)this, 0, NULL);
	return true;
}

bool CBattleServer::CreateIOCP_Socket(SOCKET Socket, ULONG_PTR Key)
{
	//	IOCP 등록
	CreateIoCompletionPort((HANDLE)Socket, _hIOCP, Key, 0);
	return true;
}

void CBattleServer::SendPacket_GameAll(CPacket *pPacket, unsigned __int64 ExcludeClientID = 0)
{

	return;
}

void CBattleServer::SendPacket(CPacket *pPacket, unsigned __int64 ClientID)
{

	return;
}

bool CBattleServer::SessionAcquireLock(int Index)
{
	if (true == _pSessionArray[Index]->_LogOutFlag)
		return false;

	if (1 == InterlockedIncrement(&_pSessionArray[Index]->_IOCount))
	{
		SessionAcquireFree(Index);
		return false;
	}
	return true;
}

bool CBattleServer::SessionAcquireFree(int Index)
{
	long IOCount = 0;
	IOCount = InterlockedDecrement(&_pSessionArray[Index]->_IOCount);
	if (0 == IOCount)
	{
		_pSessionArray[Index]->_LogOutFlag = true;
		return false;
	}
	return true;
}

void CBattleServer::Error(int ErrorCode, WCHAR *szFormatStr, ...)
{

	return;
}

void CBattleServer::StartRecvPost(int Index)
{
	DWORD Flag = 0;
	ZeroMemory(&_pSessionArray[Index]->_RecvOver, sizeof(_pSessionArray[Index]->_RecvOver));

	WSABUF Buf[2];
	DWORD FreeSize = _pSessionArray[Index]->_RecvQ.GetFreeSize();
	DWORD NotBrokenPushSize = _pSessionArray[Index]->_RecvQ.GetNotBrokenPushSize();
	if (0 == FreeSize && 0 == NotBrokenPushSize)
	{
		_pLog->Log(L"Error", LOG_SYSTEM, L"RecvPost - RecvQ Full [Index : %d]", Index);
		
		if (true == SessionAcquireFree(Index))
		{
			shutdown(_pSessionArray[Index]->_ClientInfo.Sock, SD_BOTH);
		}
		else
			return;
	}

	int NumOfBuf = (NotBrokenPushSize < FreeSize) ? 2 : 1;

	Buf[0].buf = _pSessionArray[Index]->_RecvQ.GetWriteBufferPtr();
	Buf[0].len = NotBrokenPushSize;

	if (2 == NumOfBuf)
	{
		Buf[1].buf = _pSessionArray[Index]->_RecvQ.GetBufferPtr();
		Buf[1].len = FreeSize - NotBrokenPushSize;
	}

	if (SOCKET_ERROR == WSARecv(_pSessionArray[Index]->_ClientInfo.Sock, Buf, NumOfBuf, NULL, &Flag, &_pSessionArray[Index]->_RecvOver, NULL))
	{
		int LastError = WSAGetLastError();
		if (10060 == LastError)
		{
			_pLog->Log(L"shutdown", LOG_SYSTEM, L"Recv SocketError - Code %d", LastError);
			shutdown(_pSessionArray[Index]->_ClientInfo.Sock, SD_BOTH);
		}
		else if (ERROR_IO_PENDING != LastError)
		{
			if (true != SessionAcquireFree(Index))
			{
				_pLog->Log(L"shutdown", LOG_SYSTEM, L"Recv SocketError - Code %d", LastError);
				shutdown(_pSessionArray[Index]->_ClientInfo.Sock, SD_BOTH);
			}
		}
	}
	return;
}

void CBattleServer::RecvPost(int Index)
{
	if (false == SessionAcquireLock(Index))
		return;

	CNetSession *pSession = _pSessionArray[Index];
	DWORD Flag = 0;
	ZeroMemory(&pSession->_RecvOver, sizeof(pSession->_RecvOver));
	
	WSABUF Buf[2];
	DWORD FreeSize = pSession->_RecvQ.GetFreeSize();
	DWORD NotBrokenPushSize = pSession->_RecvQ.GetNotBrokenPushSize();
	if (0 == FreeSize && 0 == NotBrokenPushSize)
	{
		_pLog->Log(L"Error", LOG_SYSTEM, L"RecvPost - RecvQ Full [Index : %d]", Index);

		if (true == SessionAcquireFree(Index))
		{
			shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
		}
		else
			return;
	}

	int NumOfBuf = (NotBrokenPushSize < FreeSize) ? 2 : 1;

	Buf[0].buf = pSession->_RecvQ.GetWriteBufferPtr();
	Buf[0].len = NotBrokenPushSize;

	if (2 == NumOfBuf)
	{
		Buf[1].buf = pSession->_RecvQ.GetBufferPtr();
		Buf[1].len = FreeSize - NotBrokenPushSize;
	}

	if (SOCKET_ERROR == WSARecv(pSession->_ClientInfo.Sock, Buf, NumOfBuf, NULL, &Flag, &pSession->_RecvOver, NULL))
	{
		int LastError = WSAGetLastError();
		if (10060 == LastError)
		{
			_pLog->Log(L"shutdown", LOG_SYSTEM, L"Recv SocketError - Code %d", LastError);
			shutdown(_pSessionArray[Index]->_ClientInfo.Sock, SD_BOTH);
		}
		else if (ERROR_IO_PENDING != LastError)
		{
			if (true != SessionAcquireFree(Index))
			{
				_pLog->Log(L"shutdown", LOG_SYSTEM, L"Recv SocketError - Code %d", LastError);
				shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
			}
		}
	}
	return;
}

void CBattleServer::SendPost(int Index)
{
	CNetSession *pSession = _pSessionArray[Index];
	if (true == InterlockedCompareExchange(&pSession->_SendFlag, true, false))
		return;

	if (0 == pSession->_SendQ.GetUseSize())
//	if (0 == _pSessionArray[Index]->_SendQ.GetUseCount())
	{
		InterlockedExchange(&pSession->_SendFlag, false);
		return;
	}

	if (CNetSession::MODE_AUTH != pSession->_Mode && CNetSession::MODE_GAME != pSession->_Mode)
	{
		InterlockedExchange(&pSession->_SendFlag, false);
		return;
	}

	WSABUF Buf[WSABUF_MAX];
	CPacket *pPacket;
	int BufNum;
	int UseSize = (pSession->_SendQ.GetUseSize() / sizeof(CPacket*));
//	int UseSize = (_pSessionArray[Index]->_SendQ.GetUseCount());
	if (UseSize > WSABUF_MAX)
	{
		BufNum = WSABUF_MAX;
		pSession->_iSendPacketCnt = WSABUF_MAX;

		_Monitor_Counter_Send+= WSABUF_MAX;
		for (int j = 0; j < WSABUF_MAX; j++)
		{
			pSession->_SendQ.Dequeue((char*)&pPacket, sizeof(CPacket*));
//			pSession->_SendQ.Dequeue(pPacket);
			pSession->_CompleteSendPacket.Enqueue((char*)&pPacket, sizeof(CPacket*));
	//		pSession->_CompleteSendPacket.Enqueue(pPacket);
			Buf[j].buf = pPacket->GetBufferPtr();
			Buf[j].len = pPacket->GetPacketSize();
		}
	}
	else
	{
		BufNum = UseSize;
		pSession->_iSendPacketCnt = UseSize;

		_Monitor_Counter_Send += UseSize;
		for (int j = 0; j < UseSize; j++)
		{
			pSession->_SendQ.Dequeue((char*)&pPacket, sizeof(CPacket*));
//			pSession->_SendQ.Dequeue(pPacket);
			pSession->_CompleteSendPacket.Enqueue((char*)&pPacket, sizeof(CPacket*));
//			pSession->_CompleteSendPacket.Enqueue(pPacket);
			Buf[j].buf = pPacket->GetBufferPtr();
			Buf[j].len = pPacket->GetPacketSize();
		}
	}

	if (false == SessionAcquireLock(Index))
	{
		InterlockedExchange(&pSession->_SendFlag, false);
		return;
	}
	ZeroMemory(&pSession->_SendOver, sizeof(pSession->_SendOver));
	if (SOCKET_ERROR == WSASend(pSession->_ClientInfo.Sock, Buf, BufNum, NULL, 0, &pSession->_SendOver, NULL))
	{
		int LastError = WSAGetLastError();
		if (10060 == LastError)
		{
			_pLog->Log(L"shutdown", LOG_SYSTEM, L"Send SocketError - Code %d", LastError);
			shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
			InterlockedExchange(&pSession->_SendFlag, false);
		}
		else if (ERROR_IO_PENDING != LastError)
		{
			SessionAcquireFree(Index);
			if (10054 != LastError)
				_pLog->Log(L"shutdown", LOG_SYSTEM, L"Send SocketError - Code %d", LastError);
			shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
			InterlockedExchange(&pSession->_SendFlag, false);
			return;

		}
	}
	return;
}

void CBattleServer::CompleteRecv(int Index, DWORD Trans)
{
	CNetSession *pSession = _pSessionArray[Index];
	pSession->_RecvQ.Enqueue(Trans);

	while (0 < pSession->_RecvQ.GetUseSize())
	{
		if (sizeof(CPacket::st_PACKET_HEADER) > pSession->_RecvQ.GetUseSize())
		{
			RecvPost(Index);
			return;
		}

		CPacket::st_PACKET_HEADER _Header;
		pSession->_RecvQ.Peek((char*)&_Header, sizeof(CPacket::st_PACKET_HEADER));
	
		if (CNetSession::BUF < _Header.shLen)
		{
			_pLog->Log(L"shutdown", LOG_SYSTEM, L"CompleteRecv - Packet Header bigger than Buf Size  Index : %d", Index);
			shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
			return;
		}
	
		if (sizeof(CPacket::st_PACKET_HEADER) + _Header.shLen > pSession->_RecvQ.GetUseSize())
		{
			RecvPost(Index);
			return;
		}

		if (_byCode != _Header.byCode)
		{
			_pLog->Log(L"shutdown", LOG_SYSTEM, L"CompleteRecv - Packet Code Not Same  Index : %d", Index);
			shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
			return;
		}

		CPacket *pPacket = CPacket::Alloc();
		pSession->_RecvQ.Dequeue((char*)pPacket->GetBufferPtr(), _Header.shLen + sizeof(CPacket::st_PACKET_HEADER));
		pPacket->PushData(_Header.shLen + sizeof(CPacket::st_PACKET_HEADER));
		if (false == pPacket->DeCode(&_Header))
		{
			_pLog->Log(L"shutdown", LOG_SYSTEM, L"CompleteRecv - Decode Error  Index : %d", Index);
			shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
			pPacket->Free();
			return;
		}
		pPacket->m_header.byCode = _Header.byCode;
		pPacket->m_header.shLen = _Header.shLen;
		pPacket->m_header.RandKey = _Header.RandKey;
		pPacket->m_header.CheckSum = _Header.CheckSum;
		pPacket->PopData(sizeof(CPacket::st_PACKET_HEADER));

		pSession->_CompleteRecvPacket.Enqueue(pPacket);

		_Monitor_Counter_Recv++;
	}
	RecvPost(Index);
	return;
}

void CBattleServer::CompleteSend(int Index, DWORD Trans)
{
	CPacket *pPacket[WSABUF_MAX];
	CNetSession *pSession = _pSessionArray[Index];
	int Num = pSession->_iSendPacketCnt;

	pSession->_CompleteSendPacket.Peek((char*)&pPacket, sizeof(CPacket*) * Num);
	for (int i = 0; i < Num; i++)
	{
//		pSession->_CompleteSendPacket.Dequeue((char*)&pPacket, sizeof(CPacket*));
//		pSession->_CompleteSendPacket.Dequeue(pPacket);
		pPacket[i]->Free();
		pSession->_CompleteSendPacket.Dequeue(sizeof(CPacket*));
	}
	pSession->_iSendPacketCnt -= Num;
	InterlockedExchange(&pSession->_SendFlag, false);

	return;
}

void CBattleServer::ProcAuth_Accept()
{
	_Monitor_SessionAuthMode++;

	int Index = NULL;
	
	AcquireSRWLockExclusive(&_Srwlock);
	_BlankSessionStack.Pop(Index);
	ReleaseSRWLockExclusive(&_Srwlock);

	if (NULL == Index)
	{
		//	스택에서 인덱스를 제대로 못 얻었을 경우
		_pLog->Log(L"Error", LOG_SYSTEM, L"ProcAuth_Accept - Stack Index Pop Error %d", Index);
		return;
	}

	// 신규 접속자 처리
	CLIENT_CONNECT_INFO *pInfo;
	_AccpetSocketQueue.Dequeue((char*)&pInfo, sizeof(CLIENT_CONNECT_INFO*));
//	_AccpetSocketQueue.Dequeue(pInfo);


	CNetSession* pSession = _pSessionArray[Index];
	pSession->_iArrayIndex = Index;
	pSession->_Mode = CNetSession::MODE_AUTH;
	pSession->_AuthToGameFlag = false;
	pSession->_iSendPacketCnt = NULL;
	pSession->_iSendPacketSize = NULL;
	pSession->_LogOutFlag = false;
	pSession->_RecvQ.Clear();

	int IOCount = InterlockedIncrement(&pSession->_IOCount);
	if (IOCount == 0)
	{
		_pLog->Log(L"Error", LOG_SYSTEM, L"IOCount Error - Index %d", Index);
	}

	pSession->_ClientInfo.ClientID = pInfo->ClientID;
	strcpy_s(pSession->_ClientInfo.IP, sizeof(pSession->_ClientInfo.IP), pInfo->IP);
	pSession->_ClientInfo.Port = pInfo->Port;
	pSession->_ClientInfo.Sock = pInfo->Sock;

	_pMemoryPool_ConnectInfo->Free(pInfo);

	CreateIOCP_Socket(pSession->_ClientInfo.Sock, Index);
	pSession->OnAuth_ClientJoin();
	StartRecvPost(Index);
	return;
}

void CBattleServer::ProcAuth_LogoutInAuth()
{
	//	LogOutFlag가 true인 세션을 LOGOUT_IN_AUTH 모드로 변경
	for (int i = 0; i < _iMaxSession; i++)
	{
		CNetSession *pSession = _pSessionArray[i];
		if (CNetSession::MODE_AUTH == pSession->_Mode && true == pSession->_LogOutFlag)
		{
			pSession->_Mode = CNetSession::MODE_LOGOUT_IN_AUTH;
			_Monitor_SessionAuthMode--;
			//	대기방에 접속해 있는 유저인지 검사
			if (NULL != pSession->_RoomNo)
			{
				OnRoomLeavePlayer(pSession->_RoomNo, pSession->_AccountNo);
			}
		}
	}
	return;
}

void CBattleServer::ProcAuth_Logout()
{
	//	LOGOUT_IN_AUTH 모드의 세션이 SendFlag가 0인 경우
	//	WAIT_LOGOUT으로 모드 변경
	for (int i = 0; i < _iMaxSession; i++)
	{
		CNetSession *pSession = _pSessionArray[i];
		if (CNetSession::MODE_LOGOUT_IN_AUTH == pSession->_Mode && false == pSession->_SendFlag)
		{
			pSession->_Mode = CNetSession::MODE_WAIT_LOGOUT;
			pSession->OnAuth_ClientLeave();
		}
	}
	return;
}

void CBattleServer::ProcAuth_AuthToGame()
{
	//	세션 배열을 돌면서 AuthToGame 변수가 true이며,
	//	Auth 모드인 세션을 AUTH_TO_GAME 모드로 변경
	for (int i = 0; i < _iMaxSession; i++)
	{
		CNetSession *pSession = _pSessionArray[i];
		if (CNetSession::MODE_AUTH == pSession->_Mode && pSession->_AuthToGameFlag)
		{
			pSession->_Mode = CNetSession::MODE_AUTH_TO_GAME;
			pSession->OnAuth_ClientLeave();
			_Monitor_SessionAuthMode--;
		}
	}
	return;
}

void CBattleServer::ProcGame_AuthToGame()
{
	//	Game 모드로 전환
	//	세션 배열을 돌면서 AUTH_TO_GAME 모드의 세션을 GAME 모드로 변경한다
	//	나중에는 모드 변경 인원을 제한
	//	현재는 테스트를 위해 무제한으로 변경
	for (int i = 0; i < _iMaxSession; i++)
	{
		CNetSession *pSession = _pSessionArray[i];
		if (CNetSession::MODE_AUTH_TO_GAME == pSession->_Mode)
		{
			pSession->_Mode = CNetSession::MODE_GAME;
			_Monitor_SessionGameMode++;
		}
	}
	return;
}

void CBattleServer::ProcGame_LogoutInGame()
{
	//	GAME 모드 LogoutFlag 처리
	//	세션 배열을 돌면서 GAME 모드에서 LogoutFlag가
	//	true인 세션을 LOGOUT_IN_GAME 모드로 변경
	for (int i = 0; i < _iMaxSession; i++)
	{
		CNetSession *pSession = _pSessionArray[i];
		if (CNetSession::MODE_GAME == pSession->_Mode && pSession->_LogOutFlag)
		{
			pSession->_Mode = CNetSession::MODE_LOGOUT_IN_GAME;
			_Monitor_SessionGameMode--;
		}
	}
	return;
}

void CBattleServer::ProcGame_Logout()
{
	//	LOGOUT_IN_GAME 모드 릴리즈 단계 돌입
	//	세션 배열을 돌면서 LOGOUT_IN_GAME 모드의 세션이
	//	SendFlag (WSASend 보내기 작업이 없다는 확인)가 0인 경우
	//	WAIT_LOGOUT으로 모드 변경
	//	virtual OnGame_ClientLeave 호출
	for (int i = 0; i < _iMaxSession; i++)
	{
		CNetSession *pSession = _pSessionArray[i];
		if (CNetSession::MODE_LOGOUT_IN_GAME == pSession->_Mode && false == pSession->_SendFlag)
		{
			pSession->_Mode = CNetSession::MODE_WAIT_LOGOUT;
			pSession->OnGame_ClientLeave();
		}
	}
	return;
}

void CBattleServer::ProcGame_Release()
{
	//	WAIT_LOGOUT 최종 릴리즈
	//	세션 배열을 돌면서 WAIT_LOGOUT 모드의 세션을 최종적으로 릴리즈 처리한다. 
	//	virtual OnGame_ClientRelease 호출
	//	NONE_MODE로 바꾸고 플래그 바꿈
	//	세션 초기화
	//	해당 세션 배열 Index를 BlankSessionStack에 넣음
	for (int i = 0; i < _iMaxSession; i++)
	{
		CNetSession *pSession = _pSessionArray[i];
		if (CNetSession::MODE_WAIT_LOGOUT == pSession->_Mode)
		{
			pSession->OnGame_ClientRelease();
			pSession->_Mode = CNetSession::MODE_NONE;
			pSession->_iArrayIndex = NULL;
			pSession->_iSendPacketCnt = NULL;
			pSession->_iSendPacketSize = NULL;
			pSession->_SendFlag = false;
			pSession->_LogOutFlag = false;
			pSession->_AuthToGameFlag = false;
			
			while (0 != pSession->_SendQ.GetUseSize())
//			while (0 != _pSessionArray[i]->_SendQ.GetUseCount())
			{
				//	애초에 큐에 패킷이 남아있으면 안됨..
				CPacket *pPacket;
				pSession->_SendQ.Dequeue((char*)&pPacket, sizeof(CPacket*));
//				_pSessionArray[i]->_SendQ.Dequeue(pPacket);
				pPacket->Free();
			}
			while (0 != pSession->_CompleteRecvPacket.GetUseCount())
			{
				//	애초에 큐에 패킷이 남아있으면 안됨..
				CPacket *pPacket;
				pSession->_CompleteRecvPacket.Dequeue(pPacket);
				pPacket->Free();
			}
			while (0 != pSession->_CompleteSendPacket.GetUseSize())
//			while (0 != _pSessionArray[i]->_CompleteSendPacket.GetUseCount())
			{
				//	애초에 큐에 패킷이 남아있으면 안됨..
				CPacket *pPacket;
				pSession->_CompleteSendPacket.Dequeue((char*)&pPacket, sizeof(CPacket*));
//				_pSessionArray[i]->_CompleteSendPacket.Dequeue(pPacket);
				pPacket->Free();
			}

			pSession->_ClientInfo.ClientID = NULL;
			pSession->_ClientInfo.Port = NULL;
			closesocket(pSession->_ClientInfo.Sock);
			pSession->_ClientInfo.Sock = INVALID_SOCKET;

			InterlockedDecrement(&_Monitor_SessionAllMode);
			AcquireSRWLockExclusive(&_Srwlock);
			_BlankSessionStack.Push(i);
			ReleaseSRWLockExclusive(&_Srwlock);
		}
	}
	return;
}

bool CBattleServer::AcceptThread_update()
{
	while (!_bShutdown)
	{
		SOCKADDR_IN ClientAddr;
		int AddrSize = sizeof(ClientAddr);
		SOCKET Sock = accept(_ListenSocket, (SOCKADDR*)&ClientAddr, &AddrSize);
		if (INVALID_SOCKET == Sock)
			continue;
		_Monitor_AcceptSocket++;
		_Monitor_AcceptTotal++;

		CLIENT_CONNECT_INFO *pInfo = _pMemoryPool_ConnectInfo->Alloc();
		//	IP가 제대로 저장되는지 체크할 것
		inet_ntop(AF_INET, &ClientAddr.sin_addr, pInfo->IP, sizeof(pInfo->IP));
		pInfo->Port = ClientAddr.sin_port;
		pInfo->Sock = Sock;
		pInfo->ClientID = _iClientIDCnt++;

//		_AccpetSocketQueue.Enqueue(pInfo);
		_AccpetSocketQueue.Enqueue((char*)&pInfo, sizeof(CLIENT_CONNECT_INFO*));
		InterlockedIncrement(&_Monitor_SessionAllMode);
	}
	return true;
}

bool CBattleServer::AuthThread_update()
{
	int Count;
	while (!_bShutdown)
	{
		Sleep(_iAuthThread);
//		Count = 0;
		_Monitor_Counter_AuthUpdate++;

		while (_AccpetSocketQueue.GetUseSize())
//		while(_AccpetSocketQueue.GetUseCount())
//		while (Count < Config.AUTH_MAX)
		{
//			if (0 != _AccpetSocketQueue.GetUseCount())
			{
				ProcAuth_Accept();				
			}
//			Count++;
		}

		for (int i = 0; i < _iMaxSession; i++)
		{
			CNetSession *pSession = _pSessionArray[i];
			if (CNetSession::MODE_AUTH == pSession->_Mode && 0 != pSession->_CompleteRecvPacket.GetUseCount())
			{
//				Count = 0;
				while (0 != pSession->_CompleteRecvPacket.GetUseCount())
//				while (Count < Config.AUTH_MAX)
				{
//					if (0 == pSession->_CompleteRecvPacket.GetUseCount())
//						break;
					CPacket *pPacket;
					pSession->_CompleteRecvPacket.Dequeue(pPacket);
					pSession->OnAuth_Packet(pPacket);
					pPacket->Free();
//					Count++;
				}
			}
		}
		OnAuth_Update();		
		ProcAuth_LogoutInAuth();
		ProcAuth_Logout();
		ProcAuth_AuthToGame();
	}
	return true;
}

bool CBattleServer::GameUpdateThread_update()
{
	int Count;
	while (!_bShutdown)
	{
		Sleep(_iGameThread);
//		Count = 0;
		_Monitor_Counter_GameUpdate++;
		ProcGame_AuthToGame();

		//	GAME 모드 세션들 패킷 처리
		//	세션 배열을 모두 돌면서 GAME 모드 세션을 확인하여
		//	해당 세션의 CompleteRecvPacket 처리요청
		//	virtual OnGame_Packet 호출
		//	기본 패킷 1개 처리, 상황에 따라 N개 처리
		for (int i = 0; i < _iMaxSession; i++)
		{
			CNetSession *pSession = _pSessionArray[i];
			if (CNetSession::MODE_GAME == pSession->_Mode && 0 != pSession->_CompleteRecvPacket.GetUseCount())
			{
//				Count = 0;
				while (0 != pSession->_CompleteRecvPacket.GetUseCount())
//				while (Count < Config.GAME_MAX)
				{
//					if (0 == pSession->_CompleteRecvPacket.GetUseCount())
//						break;
					CPacket *pPacket;
					pSession->_CompleteRecvPacket.Dequeue(pPacket);
					pSession->OnGame_Packet(pPacket);
					pPacket->Free();
//					Count++;
				}
			}
		}
		OnGame_Update();
		ProcGame_LogoutInGame();
		ProcGame_Logout();
		ProcGame_Release();
	}
	return true;
}

bool CBattleServer::IOCPWorkerThread_update()
{
	DWORD Retval;

	while (!_bShutdown)
	{
		OVERLAPPED * pOver = NULL;
		int  Index = _iMaxSession + 1;
		DWORD Trans = 0;
		
		Retval = GetQueuedCompletionStatus(_hIOCP, &Trans, (PULONG_PTR)&Index, (LPWSAOVERLAPPED*)&pOver, INFINITE);
		CNetSession *pSession = _pSessionArray[Index];
		if (nullptr == pOver)
		{
			if (_iMaxSession < Index && 0 == Trans)
			{
				PostQueuedCompletionStatus(_hIOCP, 0, 0, 0);
			}
		}
		else if (0 == Trans)
		{
			shutdown(pSession->_ClientInfo.Sock, SD_BOTH);
		}
		else if (pOver == &pSession->_RecvOver)
		{
			CompleteRecv(Index, Trans);
		}
		else if (pOver == &pSession->_SendOver)
		{
			CompleteSend(Index, Trans);
		}

		SessionAcquireFree(Index);
	}
	return true;
}

bool CBattleServer::SendThread_update()
{
	while (!_bShutdown)
	{
		Sleep(_iSendThread);
		for (int i = 0; i < _iMaxSession; i++)
		{
			CNetSession *pSession = _pSessionArray[i];
			if (CNetSession::MODE_NONE == pSession->_Mode || pSession->_SendFlag || pSession->_LogOutFlag ||
				CNetSession::MODE_LOGOUT_IN_AUTH == pSession->_Mode || CNetSession::MODE_LOGOUT_IN_GAME == pSession->_Mode ||
				CNetSession::MODE_WAIT_LOGOUT == pSession->_Mode)
				continue;
			
			SendPost(i);
		}
		_Monitor_Counter_PacketSend++;
	}
	return true;
}


#include <time.h>
#include <process.h>

#include "GameServer.h"

CGameServer::CGameServer()
{

}

CGameServer::CGameServer(int iMaxSession, int iSend, int iAuth, int iGame) : CBattleServer(iMaxSession, iSend, iAuth, iGame)
{
	srand(time(NULL));
	InitializeSRWLock(&_WaitRoom_lock);
	InitializeSRWLock(&_PlayRoom_lock);
	_BattleRoomPool = new CMemoryPool<BATTLEROOM>();
	_HttpPool = new CMemoryPool<CRingBuffer>();
	_bMonitor = true;
	_hMonitorThread = NULL;
	_hLanMonitorThread = NULL;
	_hLanMasterCheckThread = NULL;
	for (auto i = 0; i < 10; i++)
		_hHttpSendThread[i] = NULL;

	_pPlayer = new CPlayer[iMaxSession];
	for (int i = 0; i < iMaxSession; i++)
	{
		SetSessionArray(i, (CNetSession*)&_pPlayer[i]);
		_pPlayer->SetGame(this);
	}

	ZeroMemory(&_OldConnectToken, sizeof(_OldConnectToken));
	ZeroMemory(&_CurConnectToken, sizeof(_CurConnectToken));
	_CreateTokenTick = NULL;

	_pMonitor = new CLanClient;
	_pMonitor->Constructor(this);
	_pMaster = new CLanClient;
	_pMaster->Constructor(this);

	_BattleServerNo = NULL;
	_RoomCnt = 1;

	_Sequence = 1;
	_TimeStamp = NULL;

	_BattleServer_On = 1;
	_BattleServer_CPU = NULL;
	_BattleServer_Memory_Commit = NULL;
	_BattleServer_PacketPool = NULL;
	_BattleServer_Auth_FPS = NULL;
	_BattleServer_Game_FPS = NULL;
	_BattleServer_Session_ALL = NULL;
	_BattleServer_Session_Auth = NULL;
	_BattleServer_Session_Game = NULL;
	_BattleServer_Session_Login = NULL;
	_BattleServer_Room_Wait = NULL;
	_BattleServer_Room_Play = NULL;

	PdhOpenQuery(NULL, NULL, &_CpuQuery);
	PdhAddCounter(_CpuQuery, L"\\Memory\\Available MBytes", NULL, &_MemoryAvailableMBytes);
	PdhAddCounter(_CpuQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &_MemoryNonpagedBytes);
	PdhAddCounter(_CpuQuery, L"\\Process(MMOGameServer)\\Private Bytes", NULL, &_ProcessPrivateBytes);
	PdhCollectQueryData(_CpuQuery);
	
	NewConnectTokenCreate();
}

CGameServer::~CGameServer()
{
	delete[] _pPlayer;
}

bool CGameServer::ThreadInit()
{
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, &MonitorThread, (LPVOID)this, 0, NULL);
	_hLanMonitorThread = (HANDLE)_beginthreadex(NULL, 0, &LanMonitorThread, (LPVOID)this, 0, NULL);
	_hLanMasterCheckThread = (HANDLE)_beginthreadex(NULL, 0, &LanMasterCheckThread, (LPVOID)this, 0, NULL);
	for (auto i = 0; i < 10; i++)
		_hHttpSendThread[i] = (HANDLE)_beginthreadex(NULL, 0, &HttpSendThread, (LPVOID)this, 0, NULL);
	_hHttpEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	return true;
}


void CGameServer::OnConnectionRequest()
{

	return;
}

void CGameServer::OnAuth_Update()
{
	//-----------------------------------------------------------
	//	Ŭ���̾�Ʈ�� ��û(��Ŷ����)�ܿ� �⺻������ �׽�
	//	ó���Ǿ�� �� ������ �κ� ����
	//-----------------------------------------------------------
	if (true == _pMaster->IsConnect())
		WaitRoomSizeCheck();
	if (0 != _WaitRoomMap.size())
		WaitRoomReadyCheck();
	if (0 != _WaitRoomMap.size())
		WaitRoomGameReadyCheck();
	return;
}

void CGameServer::OnGame_Update()
{
	//-----------------------------------------------------------
	//	GAME ����� Update ó��
	//	Ŭ���̾�Ʈ�� ��û (��Ŷ����) �ܿ� �⺻������ �׽�
	//	ó���Ǿ�� �� ���� ������ �κ� ����
	//-----------------------------------------------------------
	
	//	�����׽�Ʈ �����̹Ƿ� �÷��� ���� �÷��� true ����
	PlayRoomGameEndChange();
	PlayRoomGameEndCheck();
	PlayRoomDestroyCheck();
	return;
}

void CGameServer::OnError(int iErrorCode, WCHAR *szError)
{

	return;
}

void CGameServer::OnRoomLeavePlayer(int RoomNo, INT64 AccountNo)
{
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_WaitRoom_lock);
	iter = _WaitRoomMap.find(RoomNo);
	//	������ �غ�Ϸ� �÷������� �˻�
	if (_WaitRoomMap.end() == iter)
		return;
	else
	{
		if (false == (*iter).second->RoomReady)
		{
			//	������ ������ ��Ʋ ������ ���濡�� ������ ���� ��Ŷ ����
			CPacket *pPacket = CPacket::Alloc();
			WORD Type = en_PACKET_BAT_MAS_REQ_LEFT_USER;
			*pPacket >> Type >> RoomNo >> AccountNo;
			_pMaster->SendPacket(pPacket);
			pPacket->Free();
			//	�ش� ���� �����鿡�� �ش� �÷��̾� �濡�� ���� ��Ŷ ����
			CPacket *newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_REMOVE_USER;
			*newPacket >> Type >> RoomNo >> AccountNo >> _Sequence;
			newPacket->AddRef();
			for (auto j = (*iter).second->RoomPlayer.begin(); j != (*iter).second->RoomPlayer.end(); j++)
			{
				if (AccountNo == (*j).AccountNo)
					continue;
				_pSessionArray[(*j).Index]->SendPacket(newPacket);
				newPacket->Free();
			}
			newPacket->Free();
		}
	}	
	ReleaseSRWLockExclusive(&_WaitRoom_lock);
}

bool CGameServer::MonitorOnOff()
{
	if (_bMonitor == true)
		_bMonitor = false;
	else
		_bMonitor = true;
	return _bMonitor;
}

bool CGameServer::MonitorThread_update()
{
	struct tm *pTime = new struct tm;
	time_t Timer;
	Timer = time(NULL);
	localtime_s(pTime, &Timer);

	int year = pTime->tm_year + 1900;
	int month = pTime->tm_mon + 1;
	int day = pTime->tm_mday;
	int hour = pTime->tm_hour;
	int min = pTime->tm_min;
	int sec = pTime->tm_sec;
	int Count = 0;
	while (!_bShutdown)
	{
		Sleep(1000);

		_Cpu.UpdateCpuTime();
		_Ethernet.Count();
		Timer = time(NULL);
		localtime_s(pTime, &Timer);

		if (true == _bMonitor)
		{
			wprintf(L"\n\n");
			wprintf(L"////////////////////////////////////////////////////////////////////////////////\n");
			wprintf(L"	ServerStart : %d/%d/%d %d:%d:%d\n\n", year, month, day, hour, min, sec);
			wprintf(L"////////////////////////////////////////////////////////////////////////////////\n");
			wprintf(L"	%d/%d/%d %d:%d:%d\n\n", pTime->tm_year + 1900, pTime->tm_mon + 1,
				pTime->tm_mday, pTime->tm_hour, pTime->tm_min, pTime->tm_sec);

			wprintf(L"	SessionALL		:	%d\n", _Monitor_SessionAllMode);
			wprintf(L"	SessionAuth 		:	%d\n", _Monitor_SessionAuthMode);
			wprintf(L"	SessionGame		:	%d\n\n", _Monitor_SessionGameMode);
			
			wprintf(L"	Recv TPS		:	%d\n", _Monitor_Counter_Recv);
			wprintf(L"	Send TPS		:	%d\n\n", _Monitor_Counter_Send);

			wprintf(L"	AcceptTotal		:	%I64d\n\n", _Monitor_AcceptTotal);
			wprintf(L"	Accept Thread FPS	:	%d\n", _Monitor_AcceptSocket);
			wprintf(L"	Send   Thread FPS	:	%d\n", _Monitor_Counter_PacketSend);
			wprintf(L"	Auth   Thread FPS	:	%d\n", _Monitor_Counter_AuthUpdate);
			wprintf(L"	Game   Thread FPS	:	%d\n\n", _Monitor_Counter_GameUpdate);

			wprintf(L"	MemoryPool Alloc	:	%I64d\n", CPacket::GetAllocPool());
			wprintf(L"	Alloc / Free		:	%d\n\n", CPacket::_UseCount);

			wprintf(L"	GameServer CPU		:	%.2f%%\n", _Cpu.ProcessTotal());
		}
		_Monitor_AcceptSocket = 0;
		_Monitor_Counter_PacketSend = 0;
		_Monitor_Counter_AuthUpdate = 0;
		_Monitor_Counter_GameUpdate = 0;
		_Monitor_Counter_Recv = 0;
		_Monitor_Counter_Send = 0;
	}
	return true;
}

bool CGameServer::LanMonitorThread_Update()
{	
	while (1)
	{
		Sleep(1000);

		if (false == _pMonitor->IsConnect())
		{
			_pMonitor->Connect(Config.MONITOR_BIND_IP, Config.MONITOR_BIND_PORT, true, Config.WORKER_THREAD);
			continue;
		}

		PdhCollectQueryData(_CpuQuery);
		PdhGetFormattedCounterValue(_ProcessPrivateBytes, PDH_FMT_DOUBLE, NULL, &_CounterVal);
		_BattleServer_Memory_Commit = (int)_CounterVal.doubleValue / (1024 * 1024);
		
		//	����͸� ������ ��Ʋ���� ����͸� �׸� ����
		MonitorSendPacket();
	}
	return true;
}

void CGameServer::MonitorSendPacket()
{
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_SERVER_CPU_TOTAL);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_SERVER_AVAILABLE_MEMORY);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_SERVER_NETWORK_RECV);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_SERVER_NETWORK_SEND);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_SERVER_NONPAGED_MEMORY);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_SERVER_ON);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_CPU);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_MEMORY_COMMIT);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_PACKET_POOL);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_AUTH_FPS);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_GAME_FPS);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_SESSION_ALL);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_SESSION_AUTH);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_BATTLE_SESSION_GAME);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_CHAT_SERVER_ON);
	MonitorSendPacketCreate(dfMONITOR_DATA_TYPE_MATCH_SERVER_ON);
	return;
}

bool CGameServer::MonitorSendPacketCreate(BYTE DataType)
{
	struct tm *pTime = new struct tm;
	time_t Now;
	localtime_s(pTime, &Now);
	_TimeStamp = time(NULL);
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	CPacket *pPacket = CPacket::Alloc();
	*pPacket << Type << DataType;
	switch (DataType)
	{
	case dfMONITOR_DATA_TYPE_BATTLE_SERVER_ON:
	{		
		*pPacket << _BattleServer_On;
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_CPU:
	{
		_BattleServer_CPU = _Cpu.ProcessTotal();		
		*pPacket << _BattleServer_CPU;
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_MEMORY_COMMIT:
	{
		//	�޸� Ŀ��(private)MByte		
		*pPacket << _BattleServer_Memory_Commit;
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_PACKET_POOL:
	{
		_BattleServer_PacketPool = CPacket::m_pMemoryPool->_UseCount;	
		*pPacket << _BattleServer_PacketPool;
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_AUTH_FPS:
	{
		_BattleServer_Auth_FPS = _Monitor_Counter_AuthThreadAvr;
		*pPacket << _BattleServer_Auth_FPS;	
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_GAME_FPS:
	{
		_BattleServer_Game_FPS = _Monitor_Counter_GameThreadAvr;
		*pPacket << _BattleServer_Game_FPS;	
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_SESSION_ALL:
	{
		_BattleServer_Session_ALL = _Monitor_SessionAllMode;
		*pPacket << _BattleServer_Session_ALL;		
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_SESSION_AUTH:
	{
		_BattleServer_Session_Auth = _Monitor_SessionAuthMode;
		*pPacket << _BattleServer_Session_Auth;
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_SESSION_GAME:
	{
		_BattleServer_Session_Game = _Monitor_SessionGameMode;
		*pPacket << _BattleServer_Session_Game;
	}
	break;
	//case dfMONITOR_DATA_TYPE_BATTLE_:
	//{
	//	//	���� - �̻��
	//}
	//break;
	case dfMONITOR_DATA_TYPE_BATTLE_ROOM_WAIT:
	{
		_BattleServer_Room_Wait = _WaitRoomMap.size();
		*pPacket << _BattleServer_Room_Wait;	
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_ROOM_PLAY:
	{
		_BattleServer_Room_Play = _PlayRoomMap.size();
		*pPacket << _BattleServer_Room_Play;
	}
	break;
	default:
		//	�Ұ����� ��Ȳ
		g_CrashDump->Crash();
		break;
	}
	*pPacket << _TimeStamp;
	_pMonitor->SendPacket(pPacket);
	pPacket->Free();
	delete pTime;
	return true;
}

void CGameServer::EntertokenCreate(char *pBuff)
{
	for (int i = 0; i < 32; i++)
		pBuff[i] = rand() % 26 + 97;
	return;
}

void CGameServer::NewConnectTokenCreate()
{
	strcpy_s(_OldConnectToken, sizeof(_OldConnectToken), _CurConnectToken);

	for (int i = 0; i < 32; i++)
		_CurConnectToken[i] = rand() % 26 + 97;
	_CreateTokenTick = GetTickCount64();

	return;
}

void CGameServer::LanMasterCheckThead_Update()
{
	//-------------------------------------------------------------
	//	������ ���� ���� �ֱ������� Ȯ��
	//	ConnectToken ����� ���
	//-------------------------------------------------------------
	UINT64 start = GetTickCount64();
	UINT64 now = NULL;
	while (1)
	{
		Sleep(5000);
		now = GetTickCount64();
		if (now - start > Config.CONNECTTOKEN_RECREATE)
		{
			NewConnectTokenCreate();
			start = now;
			CPacket * pPacket = CPacket::Alloc();
			WORD Type = en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN;
			*pPacket >> Type;
			pPacket->PushData((char*)&_CurConnectToken, sizeof(_CurConnectToken));
			*pPacket >> _Sequence;
			_pMaster->SendPacket(pPacket);
			pPacket->Free();
		}

		if (false == _pMaster->IsConnect())
		{
			_pMaster->Connect(Config.MASTER_BIND_IP, Config.MASTER_BIND_PORT, true, LANCLIENT_WORKERTHREAD);
			continue;
		}
	}
	return;
}

void CGameServer::HttpSendThread_Update()
{
	//-------------------------------------------------------------
	//	�̺�Ʈ�� �����带 ���� �� HttpQueue�� �ִ� ������ ó��
	//	ó���� �����Ͱ� ���� ���� ��� ���ѷ��� 
	//-------------------------------------------------------------
	while (1)
	{
		WaitForSingleObject(_hHttpEvent, INFINITE);
		while (0 < _HttpQueue.GetUseCount())
		{
			CRingBuffer * pBuffer;
			_HttpQueue.Dequeue(pBuffer);
			if (nullptr == pBuffer)
				break;
			WORD Type;
			pBuffer->Dequeue((char*)&Type, sizeof(Type));
			
			switch (Type)
			{
			case SELECT:
			{
				HttpSend_Select(pBuffer);
			}
			break;
			case UPDATE:
			{
				HttpSend_Update(pBuffer);
			}
			break;
			default:
			{
				wprintf(L"Wrong Packet Type !!\n");
				g_CrashDump->Crash();
				break;
			}
			}
		}
	}
}

void CGameServer::HttpSend_Select(CRingBuffer * pBuffer)
{
	//-----------------------------------------------------------
	//	select_account.php ȣ��
	//-----------------------------------------------------------
	int Index = NULL;
	INT64 AccountNo = NULL;
	pBuffer->Dequeue((char*)&Index, sizeof(Index));
	pBuffer->Dequeue((char*)&AccountNo, sizeof(AccountNo));
	//	Set Post Data
	Json::Value PostData;
	PostData["accountno"] = AccountNo;
	string temp = HttpPacket_Create(PostData);
	//	Result Check
	if (false == _pSessionArray[Index]->OnHttp_Result_SelectAccount(temp))
		return;
	temp.clear();
	//-----------------------------------------------------------
	//	select_contents.php ȣ��
	//-----------------------------------------------------------
	temp = HttpPacket_Create(PostData);
	//	Result Check
	if (false == _pSessionArray[Index]->OnHttp_Result_SelectContents(temp))
		return;

	//	���� ��Ŷ ����
	_pSessionArray[Index]->OnHttp_Result_Success();
	return;
}

void CGameServer::HttpSend_Update(CRingBuffer * pBuffer)
{
	
	return;
}

string CGameServer::HttpPacket_Create(Json::Value PostData)
{
	Json::StyledWriter writer;
	string Data = writer.write(PostData);
	WinHttpClient HttpClient(Config.APISERVER_SELECT_ACCOUNT);
	HttpClient.SetAdditionalDataToSend((BYTE*)Data.c_str(), Data.size());
	//	Set Request Header
	wchar_t szSize[50] = L"";
	swprintf_s(szSize, L"%d", Data.size());
	wstring Headers = L"Content-Length: ";
	Headers += szSize;
	Headers += L"\r\nContent-Type: application/x-www-form-urlencoded\r\n";
	HttpClient.SetAdditionalRequestHeaders(Headers);
	//	Send HTTP post request
	HttpClient.SendHttpRequest(L"POST");
	//	Response wstring -> string convert
	wstring response = HttpClient.GetResponseContent();
	string temp;
	temp.assign(response.begin(), response.end());
	
	return temp;
}

void CGameServer::WaitRoomSizeCheck()
{
	//-----------------------------------------------------------
	//	���� ���� üũ �� Default ������ ���� ��� �� ���� ȣ��
	//-----------------------------------------------------------
	while (Config.BATTLEROOM_DEFAULT_NUM > _WaitRoomMap.size())
	{
		WaitRoomCreate();
	}
	return;
}

void CGameServer::WaitRoomCreate()
{
	//-----------------------------------------------------------
	//	���� �����ϰ� ������ ������ �뺸
	//-----------------------------------------------------------
	BATTLEROOM * Room = _BattleRoomPool->Alloc();
	EntertokenCreate(Room->Entertoken);
	Room->CurUser = 0;
	Room->MaxUser = Config.BATTLEROOM_MAX_USER;
	Room->RoomNo = _RoomCnt++;
	Room->ReadyCount = NULL;
	Room->RoomReady = false;
	Room->PlayReady = false;
	Room->GameEnd = false;
	AcquireSRWLockExclusive(&_WaitRoom_lock);
	_WaitRoomMap.insert(make_pair(Room->RoomNo, Room));
	ReleaseSRWLockExclusive(&_WaitRoom_lock);

	CPacket *pPacket = CPacket::Alloc();
	WORD Type = en_PACKET_BAT_MAS_REQ_CREATED_ROOM;
	*pPacket << Type << _BattleServerNo << Room->RoomNo << Room->MaxUser;
	pPacket->PushData((char*)&Room->Entertoken, sizeof(Room->Entertoken));
	*pPacket >> _Sequence;
	_pMaster->SendPacket(pPacket);
	pPacket->Free();
	return;
}


void CGameServer::WaitRoomReadyCheck()
{
	//-----------------------------------------------------------
	//	���� �غ�Ϸ� �÷��׸� �˻��Ͽ� �÷��װ� TRUE�� ���� 
	//	���� �����غ� �� ��ȯ �÷��� ���� �� ����ð� Count ������ ����
	//	�濡 �ִ� ��� �ο����� ���� �÷��� �غ� ��Ŷ ����
	//-----------------------------------------------------------
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_WaitRoom_lock);
	for (iter = _WaitRoomMap.begin(); iter != _WaitRoomMap.end(); iter++)
	{
		if (true == (*iter).second->RoomReady)
		{
			(*iter).second->PlayReady = true;
			(*iter).second->ReadyCount = GetTickCount64();
			CPacket * pPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_GAME_RES_PLAY_READY;
			BYTE ReadySec = Config.BATTLEROOM_READYSEC;
			*pPacket >> Type >> (*iter).second->RoomNo >> ReadySec;
			pPacket->AddRef();
			for (auto j = (*iter).second->RoomPlayer.begin(); j != (*iter).second->RoomPlayer.end(); j++)
			{
				_pSessionArray[(*j).Index]->SendPacket(pPacket);
				pPacket->Free();
			}
			pPacket->Free();
			continue;
		}
	}
	ReleaseSRWLockExclusive(&_WaitRoom_lock);
	return;
}

void CGameServer::WaitRoomGameReadyCheck()
{
	//-----------------------------------------------------------
	//	�غ� �� �߿��� (�÷��� �غ� �÷��� - TRUE ) ���� �ð��� Config���� ���� �غ�ð����� Ŭ ��� 
	//	WaitRoomMap���� �ش� ���� ���� �� PlayRoomMap�� �߰� �� �ش� ���� ��� �ο����� �÷��� ���� ��Ŷ ����
	//-----------------------------------------------------------
	__int64 now = GetTickCount64();
	std::map<int, BATTLEROOM*> TempMap;
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_WaitRoom_lock);
	for (iter = _WaitRoomMap.begin(); iter != _WaitRoomMap.end();)
	{
		if (true == (*iter).second->PlayReady && now - (*iter).second->ReadyCount > (Config.BATTLEROOM_READYSEC * 1000))
		{
			TempMap.insert(*iter);
			_WaitRoomMap.erase(iter++);
		}
		else
			iter++;
	}
	ReleaseSRWLockExclusive(&_WaitRoom_lock);
	//	TempMap�� �����鿡�� ��Ŷ�� ���� �� PlayMap�� �߰�
	for (iter = TempMap.begin(); iter != TempMap.end();)
	{
		CPacket * pPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_PLAY_START;
		*pPacket >> Type >> (*iter).second->RoomNo;
		pPacket->AddRef();
		for (auto i = (*iter).second->RoomPlayer.begin(); i != (*iter).second->RoomPlayer.end(); i++)
		{
			_pSessionArray[(*i).Index]->_AuthToGameFlag = true;
			_pSessionArray[(*i).Index]->SendPacket(pPacket);
			pPacket->Free();
		}
		pPacket->Free();
		AcquireSRWLockExclusive(&_PlayRoom_lock);
		_PlayRoomMap.insert(*iter);
		ReleaseSRWLockExclusive(&_PlayRoom_lock);
		TempMap.erase(iter++);
	}
	return;
}

void CGameServer::PlayRoomGameEndCheck()
{
	//-----------------------------------------------------------
	//	������ �������� ������ ������ �ʴ� ��� ������ ����
	//-----------------------------------------------------------
	AcquireSRWLockExclusive(&_PlayRoom_lock);
	for (auto i = _PlayRoomMap.begin(); i != _PlayRoomMap.end(); i++)
	{
		if (true == (*i).second->GameEnd)
		{
			for (auto j = (*i).second->RoomPlayer.begin(); j != (*i).second->RoomPlayer.end();)
			{
				_pSessionArray[(*j).Index]->Disconnect();
//				(*i).second->RoomPlayer.erase(j);
			}
		}
	}
	ReleaseSRWLockExclusive(&_PlayRoom_lock);
}

void CGameServer::PlayRoomDestroyCheck()
{
	//-----------------------------------------------------------
	//	�濡 ������ ���� ��� ���� �ı� 
	//-----------------------------------------------------------
	AcquireSRWLockExclusive(&_PlayRoom_lock);
	for (auto i = _PlayRoomMap.begin(); i != _PlayRoomMap.end();)
	{
		if (0 == (*i).second->RoomPlayer.size())
		{
			_PlayRoomMap.erase(i);
		}
		else
			i++;
	}
	ReleaseSRWLockExclusive(&_PlayRoom_lock);
	return;
}

void CGameServer::PlayRoomGameEndChange()
{
	AcquireSRWLockExclusive(&_PlayRoom_lock);
	for (auto i = _PlayRoomMap.begin(); i != _PlayRoomMap.end(); i++)
	{
		(*i).second->GameEnd = true;
	}
	ReleaseSRWLockExclusive(&_PlayRoom_lock);
	return;
}

























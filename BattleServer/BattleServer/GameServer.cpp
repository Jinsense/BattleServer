#include <time.h>
#include <process.h>

#include "GameServer.h"

CGameServer::CGameServer()
{

}

CGameServer::CGameServer(int iMaxSession, int iSend, int iAuth, int iGame) : CBattleServer(iMaxSession, iSend, iAuth, iGame)
{
	srand(time(NULL));
	InitializeSRWLock(&_HttpReq_lock);
	InitializeSRWLock(&_WaitRoom_lock);
	InitializeSRWLock(&_PlayRoom_lock);
	InitializeSRWLock(&_ClosedRoom_lock);
	InitializeSRWLock(&_RoomPlayer_lock);
	_BattleRoomPool = new CMemoryPool<BATTLEROOM>();
	_RoomPlayerPool = new CMemoryPool<RoomPlayerInfo>();
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
		_pPlayer[i].SetGame(this);
	}

	ZeroMemory(&_ChatServerIP, sizeof(_ChatServerIP));
	_ChatServerPort = NULL;

	ZeroMemory(&_OldConnectToken, sizeof(_OldConnectToken));
	ZeroMemory(&_CurConnectToken, sizeof(_CurConnectToken));
	_CreateTokenTick = NULL;
	NewConnectTokenCreate();
	memcpy(_OldConnectToken, _CurConnectToken, sizeof(_CurConnectToken));

	_bChatConnect = false;

	_pChat = new CChatLanServer;
	_pChat->Set(this);

	_pMaster = new CLanMasterClient;
	_pMaster->Constructor(this);

	_pMonitor = new CLanMonitorClient;
	_pMonitor->Constructor(this);

	_BattleServerNo = NULL;
	_RoomCnt = 1;
	_WaitRoomCount = NULL;
	_CountDownRoomCount = NULL;
	_PlayRoomCount = NULL;

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
	//	클라이언트의 요청(패킷수신)외에 기본적으로 항시
	//	처리되어야 할 컨텐츠 부분 로직
	//-----------------------------------------------------------
	if (true == _pMaster->IsConnect())
		WaitRoomSizeCheck();
	WaitRoomReadyCheck();
	WaitRoomGameReadyCheck();
	return;
}

void CGameServer::OnGame_Update()
{
	//-----------------------------------------------------------
	//	GAME 모드의 Update 처리
	//	클라이언트의 요청 (패킷수신) 외에 기본적으로 항시
	//	처리되어야 할 게임 컨텐츠 부분 로직
	//-----------------------------------------------------------
	PlayRoomGameEnd();
	PlayRoomGameEndCheck();
	PlayRoomDestroyCheck();
	return;
}

void CGameServer::OnError(int iErrorCode, WCHAR *szError)
{

	return;
}

bool CGameServer::OnHttpReqRemove(INT64 AccountNo)
{
	bool Find = false;
	//	맵에서 해당 AccountNo가 있을 경우 삭제
	AcquireSRWLockExclusive(&_HttpReq_lock);
	if (_HttpReqMap.find(AccountNo) == _HttpReqMap.end()) {
		// not found
		Find = false;
	}
	else {
		// found
		_HttpReqMap.erase(AccountNo);
		Find = true;
	}
	ReleaseSRWLockExclusive(&_HttpReq_lock);
	return Find;
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
			
			wprintf(L"	GameServer CPU		:	%.2f%%\n", _Cpu.ProcessTotal());
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

			wprintf(L"	WaitRoom		:	%d%\n", _WaitRoomCount);
			wprintf(L"	CountDownRoom		:	%d%\n", _CountDownRoomCount);
			wprintf(L"	PlayRoom		:	%d%\n", _PlayRoomCount);
			wprintf(L"	BattleRoomPool UseCount	:	%d%\n", _BattleRoomPool->GetUseCount());
			wprintf(L"	RoomPlayerPool UseCount	:	%d%\n", _RoomPlayerPool->GetUseCount());
			wprintf(L"	HttpPool UseCount	:	%d%\n", _HttpPool->GetUseCount());
			wprintf(L"	InfoPool UseCount	:	%d%\n", ConnectInfo_UseCount());
			wprintf(L"	HttpReqMap Size		:	%d%\n\n", _HttpReqMap.size());
			
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
			_pMonitor->Connect(Config.MONITOR_BIND_IP, Config.MONITOR_BIND_PORT, true, LANCLIENT_WORKERTHREAD);
			continue;
		}

		PdhCollectQueryData(_CpuQuery);
		PdhGetFormattedCounterValue(_ProcessPrivateBytes, PDH_FMT_DOUBLE, NULL, &_CounterVal);
		_BattleServer_Memory_Commit = (int)_CounterVal.doubleValue / (1024 * 1024);
		
		//	모니터링 서버로 배틀서버 모니터링 항목 전송
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
		//	메모리 커밋(private)MByte		
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
	//	//	삭제 - 미사용
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
		//	불가능한 상황
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
	memcpy(_OldConnectToken, _CurConnectToken, sizeof(_CurConnectToken));

	for (int i = 0; i < 32; i++)
		_CurConnectToken[i] = rand() % 26 + 97;
	_CreateTokenTick = GetTickCount64();

	return;
}

void CGameServer::LanMasterCheckThead_Update()
{
	//-------------------------------------------------------------
	//	마스터 서버 연결 주기적으로 확인
	//	ConnectToken 재발행 담당
	//-------------------------------------------------------------
	UINT64 start = GetTickCount64();
	UINT64 now = NULL;
	while (1)
	{
		Sleep(1000);
		now = GetTickCount64();
		if (now - start > Config.CONNECTTOKEN_RECREATE)
		{
			NewConnectTokenCreate();
			start = now;

			if (true == _bChatConnect)
			{
				//	채팅서버에 재발행 알림
				CPacket * pChatPacket = CPacket::Alloc();
				WORD Type = en_PACKET_CHAT_BAT_REQ_CONNECT_TOKEN;
				*pChatPacket << Type;
				pChatPacket->PushData((char*)&_CurConnectToken, sizeof(_CurConnectToken));
				*pChatPacket << _Sequence;
				_pChat->SendPacket(_ChatClientID, pChatPacket);
				pChatPacket->Free();
			}

			if (true == _pMaster->IsConnect())
			{
				//	마스터 서버에 재발행 알림
				CPacket * pMasterPacket = CPacket::Alloc();
				WORD Type = en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN;
				*pMasterPacket << Type;
				pMasterPacket->PushData((char*)&_CurConnectToken, sizeof(_CurConnectToken));
				*pMasterPacket << _Sequence;
				_pMaster->SendPacket(pMasterPacket);
				pMasterPacket->Free();
			}
		}

		if (false == _pMaster->IsConnect() && true == _bChatConnect)
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
	//	이벤트로 스레드를 깨운 후 HttpQueue에 있는 데이터 처리
	//	처리할 데이터가 남아 있을 경우 무한루프 
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
			pBuffer->Clear();
			_HttpPool->Free(pBuffer);
		}
	}
}

void CGameServer::HttpSend_Select(CRingBuffer * pBuffer)
{
	//-----------------------------------------------------------
	//	select_account.php 호출
	//-----------------------------------------------------------
	int Count = 0;
	int Index = NULL;
	INT64 AccountNo = NULL;
	unsigned __int64 ClientID = NULL;
	pBuffer->Dequeue((char*)&Index, sizeof(Index));
	pBuffer->Dequeue((char*)&AccountNo, sizeof(AccountNo));
	pBuffer->Dequeue((char*)&ClientID, sizeof(ClientID));

	if (0 == ClientID)
		return;

	//	Set Post Data
	Json::Value PostData;
	PostData["accountno"] = AccountNo;
	string temp;
	Json::Reader reader;
	Json::Value result;
	while (Count < 50)
	{
		temp = HttpPacket_Create(PostData);
		bool Res = reader.parse(temp, result);
		if (!Res)
			Count++;
		else
			break;	
	}	
	//	Result Check
	if (false == _pSessionArray[Index]->OnHttp_Result_SelectAccount(temp, ClientID))
		return;
	temp.clear();
	Count = 0;
	//-----------------------------------------------------------
	//	select_contents.php 호출
	//-----------------------------------------------------------
	while (Count < 50)
	{
		temp = HttpPacket_Create(PostData);
		bool Res = reader.parse(temp, result);
		if (!Res)
			Count++;
		else
			break;
	}
	//	Result Check
	if (false == _pSessionArray[Index]->OnHttp_Result_SelectContents(temp, ClientID))
		return;

	//	성공 패킷 응답
	_pSessionArray[Index]->OnHttp_Result_Success(ClientID);
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
	//	대기방 갯수 체크 후 Default 값보다 적을 경우 방 생성 호출
	//-----------------------------------------------------------
	while (Config.BATTLEROOM_DEFAULT_NUM > _WaitRoomCount)
	{
		WaitRoomCreate();
	}
	return;
}

void CGameServer::WaitRoomCreate()
{
	//-----------------------------------------------------------
	//	방을 생성하고 채팅 서버에 통보
	//	채팅서버에서 응답이 오면 마스터 서버에 통보
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
	_WaitRoomMap.insert(pair<int, BATTLEROOM*>(Room->RoomNo, Room));
	ReleaseSRWLockExclusive(&_WaitRoom_lock);
	InterlockedIncrement(&_WaitRoomCount);
	
	CPacket *pPacket = CPacket::Alloc();
	WORD Type = en_PACKET_CHAT_BAT_REQ_CREATED_ROOM;
	*pPacket << Type << _BattleServerNo << Room->RoomNo << Room->MaxUser;
	pPacket->PushData((char*)&Room->Entertoken, sizeof(Room->Entertoken));
	*pPacket << _Sequence;
	_pChat->SendPacket(_ChatClientID, pPacket);
	pPacket->Free();

	return;
}


void CGameServer::WaitRoomReadyCheck()
{
	//-----------------------------------------------------------
	//	대기방 준비완료 플래그를 검사하여 플래그가 TRUE인 방을 
	//	게임 시작준비 방 전환 플래그 변경 및 변경시간 Count 변수에 저장
	//	방에 있는 모든 인원에게 대기방 플레이 준비 패킷 전송
	//-----------------------------------------------------------
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_WaitRoom_lock);
	for (iter = _WaitRoomMap.begin(); iter != _WaitRoomMap.end();)
	{
		if (true == (*iter).second->RoomReady && false == (*iter).second->PlayReady)
		{
			////	마스터 서버로 대기 방 닫힘 패킷 전송
			//AcquireSRWLockExclusive(&_ClosedRoom_lock);
			//_ClosedRoomlist.push_back((*iter).second->RoomNo);
			//ReleaseSRWLockExclusive(&_ClosedRoom_lock);

			//CPacket * CloseRoomPacket = CPacket::Alloc();
			//WORD Type = en_PACKET_BAT_MAS_REQ_CLOSED_ROOM;
			//*CloseRoomPacket << Type << (*iter).second->RoomNo << _Sequence;
			//_pMaster->SendPacket(CloseRoomPacket);
			//CloseRoomPacket->Free();

			(*iter).second->PlayReady = true;
			(*iter).second->ReadyCount = GetTickCount64();

			InterlockedIncrement(&_CountDownRoomCount);
			InterlockedDecrement(&_WaitRoomCount);

			WORD Type = en_PACKET_CS_GAME_RES_PLAY_READY;
			BYTE ReadySec = Config.BATTLEROOM_READYSEC;

			for (auto j = (*iter).second->RoomPlayer.begin(); j != (*iter).second->RoomPlayer.end(); j++)
			{
				CPacket * pPacket = CPacket::Alloc();
				*pPacket << Type << (*iter).second->RoomNo << ReadySec;
				_pSessionArray[(*j)->Index]->SendPacket(pPacket);
				pPacket->Free();
			}
		}
		else
			iter++;
	}
	ReleaseSRWLockExclusive(&_WaitRoom_lock);


	return;
}

void CGameServer::WaitRoomGameReadyCheck()
{
	//-----------------------------------------------------------
	//	준비 방 중에서 (플레이 준비 플래그 - TRUE ) 변경 시간이 Config에서 얻어온 준비시간보다 클 경우 
	//	WaitRoomMap에서 해당 방을 삭제 후 PlayRoomMap에 추가 및 해당 방의 모든 인원에게 플레이 시작 패킷 전송
	//-----------------------------------------------------------
	__int64 now = GetTickCount64();	
	std::map<int, BATTLEROOM*>::iterator iter;	
	_TempMap.clear();
	AcquireSRWLockExclusive(&_WaitRoom_lock);
	for (iter = _WaitRoomMap.begin(); iter != _WaitRoomMap.end();)
	{
		if (true == (*iter).second->PlayReady && now - (*iter).second->ReadyCount > (Config.BATTLEROOM_READYSEC * 1000))
		{
			AcquireSRWLockExclusive(&_PlayRoom_lock);
			_PlayRoomMap.insert(*iter);
			ReleaseSRWLockExclusive(&_PlayRoom_lock);
			InterlockedIncrement(&_PlayRoomCount);

			_TempMap.insert(*iter);
			iter = _WaitRoomMap.erase(iter);
			InterlockedDecrement(&_CountDownRoomCount);
		}
		else
			iter++;
	}
	ReleaseSRWLockExclusive(&_WaitRoom_lock);

//	TempMap의 유저들에게 패킷을 전송 추가
	for (iter = _TempMap.begin(); iter != _TempMap.end();)
	{
		WORD Type = en_PACKET_CS_GAME_RES_PLAY_START;
		
		/*AcquireSRWLockExclusive(&_PlayRoom_lock);
		_PlayRoomMap.insert(*iter);
		ReleaseSRWLockExclusive(&_PlayRoom_lock);*/
		for (auto i = (*iter).second->RoomPlayer.begin(); i != (*iter).second->RoomPlayer.end(); i++)
		{
			CPacket * pPacket = CPacket::Alloc();
			*pPacket << Type << (*iter).second->RoomNo;
			_pSessionArray[(*i)->Index]->_AuthToGameFlag = true;
			_pSessionArray[(*i)->Index]->SendPacket(pPacket);
			pPacket->Free();
		}
//		InterlockedIncrement(&_PlayRoomCount);
		iter = _TempMap.erase(iter);
	}
//	ReleaseSRWLockExclusive(&_WaitRoom_lock);
	
	return;
}

void CGameServer::PlayRoomGameEnd()
{
	__int64 now = GetTickCount64();
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_PlayRoom_lock);
	for (iter = _PlayRoomMap.begin(); iter != _PlayRoomMap.end(); iter++)
	{
		if (now - (*iter).second->ReadyCount > 60000)
		{
			(*iter).second->GameEnd = true;
		}
	}
	ReleaseSRWLockExclusive(&_PlayRoom_lock);
	return;
}

void CGameServer::PlayRoomGameEndCheck()
{
	//-----------------------------------------------------------
	//	게임이 끝났으나 유저가 나가지 않는 경우 강제로 끊음
	//-----------------------------------------------------------
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_PlayRoom_lock);
	for (iter = _PlayRoomMap.begin(); iter != _PlayRoomMap.end(); iter++)
	{
		if (true == (*iter).second->GameEnd)
		{
			std::list<RoomPlayerInfo*>::iterator Room_iter;
			for (Room_iter = (*iter).second->RoomPlayer.begin(); Room_iter != (*iter).second->RoomPlayer.end(); Room_iter++)
			{
				_pLog->Log(const_cast<WCHAR*>(L"GameEndCheck"), LOG_SYSTEM, const_cast<WCHAR*>(L"[RoomNo : %d] AccountNo : %d"), (*iter).second->RoomNo, (*Room_iter)->AccountNo);

				_pSessionArray[(*Room_iter)->Index]->Disconnect();
				_RoomPlayerPool->Free(*Room_iter);
				Room_iter = (*iter).second->RoomPlayer.erase(Room_iter);
			}
		}
	}
	ReleaseSRWLockExclusive(&_PlayRoom_lock);
	return;
}

void CGameServer::PlayRoomDestroyCheck()
{
	//-----------------------------------------------------------
	//	방에 유저가 없을 경우 방을 파괴 
	//-----------------------------------------------------------
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_PlayRoom_lock);
	for (iter = _PlayRoomMap.begin(); iter != _PlayRoomMap.end();)
	{
		if (0 == (*iter).second->RoomPlayer.size())
		{
			//	채팅서버에 해당 방이 파괴됨을 전송
			CPacket * pPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CHAT_BAT_REQ_DESTROY_ROOM;
			*pPacket << Type << _BattleServerNo << (*iter).second->RoomNo << _Sequence;
			_pChat->SendPacket(_ChatClientID, pPacket);
			pPacket->Free();

			_BattleRoomPool->Free((*iter).second);
			iter = _PlayRoomMap.erase(iter);
			InterlockedDecrement(&_PlayRoomCount);
		}
		else
			iter++;
			
	}
	ReleaseSRWLockExclusive(&_PlayRoom_lock);
	return;
}





















#include <time.h>
#include <process.h>

#include "GameServer.h"

CGameServer::CGameServer(int iMaxSession, int iSend, int iAuth, int iGame) : CBattleServer(iMaxSession, iSend, iAuth, iGame)
{
	_bMonitor = true;
	_hMonitorThread = NULL;
	_pPlayer = new CPlayer[iMaxSession];
	for (int i = 0; i < iMaxSession; i++)
		SetSessionArray(i, (CNetSession*)&_pPlayer[i]);

	_TimeStamp = NULL;
	_CPU_Total = NULL;
	_Available_Memory = NULL;
	_Network_Recv = NULL;
	_Network_Send = NULL;
	_Nonpaged_Memory = NULL;

	_BattleServer_On = 1;
	_BattleServer_CPU = NULL;
	_BattleServer_Memory_Commit = NULL;
	_BattleServer_PacketPool = NULL;
	_BattleServer_Auth_FPS = NULL;
	_BattleServer_Game_FPS = NULL;
	_BattleServer_Session_ALL = NULL;
	_BattleServer_Session_Auth = NULL;
	_BattleServer_Session_Game = NULL;

	_pMonitor = new CLanClient;
	_pMonitor->Constructor(this);

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

void CGameServer::OnConnectionRequest()
{

	return;
}

void CGameServer::OnAuth_Update()
{
	//	클라이언트의 요청(패킷수신)외에 기본적으로 항시
	//	처리되어야 할 컨텐츠 부분 로직
	
	return;
}

void CGameServer::OnGame_Update()
{
	//	GAME 모드의 Update 처리
	//	클라이언트의 요청 (패킷수신) 외에 기본적으로 항시
	//	처리되어야 할 게임 컨텐츠 부분 로직

	return;
}

void CGameServer::OnError(int iErrorCode, WCHAR *szError)
{

	return;
}

bool CGameServer::MonitorInit()
{
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, &MonitorThread, (LPVOID)this, 0, NULL);
	_hLanMonitorThread = (HANDLE)_beginthreadex(NULL, 0, &LanMonitorThread, (LPVOID)this, 0, NULL);
	return true;
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
			_Monitor_Counter_Recv = 0;
			_Monitor_Counter_Send = 0;
//			wprintf(L"	Recv TPS		:	%d		Recv Avr : %d\n", _Monitor_Counter_Recv, _Monitor_Counter_RecvAvr);
//			wprintf(L"	Send TPS		:	%d		Send Avr : %d\n\n", _Monitor_Counter_Send, _Monitor_Counter_SendAvr);

			wprintf(L"	AcceptTotal		:	%I64d\n\n", _Monitor_AcceptTotal);

			wprintf(L"	Accept Thread FPS	:	%d		Accept Avr : %d\n", _Monitor_AcceptSocket, _Monitor_Counter_AcceptThreadAvr);
			wprintf(L"	Send   Thread FPS	:	%d		Send   Avr : %d\n", _Monitor_Counter_PacketSend, _Monitor_Counter_SendThreadAvr);
			wprintf(L"	Auth   Thread FPS	:	%d		Auth   Avr : %d\n", _Monitor_Counter_AuthUpdate, _Monitor_Counter_AuthThreadAvr);
			wprintf(L"	Game   Thread FPS	:	%d		Game   Avr : %d\n\n", _Monitor_Counter_GameUpdate, _Monitor_Counter_GameThreadAvr);

			wprintf(L"	MemoryPool Alloc	:	%I64d\n", CPacket::GetAllocPool());
			wprintf(L"	Alloc / Free		:	%d\n\n", CPacket::_UseCount);

			wprintf(L"	CPU Total		:	%.2f%%\n", _Cpu.ProcessorTotal());
			wprintf(L"	GameServer CPU		:	%.2f%%\n", _Cpu.ProcessTotal());
			wprintf(L"	Ethernet Recv KBytes	:	%.2f		Avr : %d\n", _Ethernet._pdh_value_Network_RecvBytes / (1024), _Monitor_Counter_NetworkRecvAvr);
			wprintf(L"	Ethernet Send KBytes	:	%.2f		Avr : %d\n", _Ethernet._pdh_value_Network_SendBytes / (1024), _Monitor_Counter_NetworkSendAvr);
		}
		_Monitor_NetworkRecvBytes[Count] = _Ethernet._pdh_value_Network_RecvBytes / (1024);
		_Monitor_NetworkSendBytes[Count] = _Ethernet._pdh_value_Network_SendBytes / (1024);
//		_Monitor_RecvAvr[Count] = _Monitor_Counter_Recv;
//		_Monitor_SendAvr[Count] = _Monitor_Counter_Send;
		_Monitor_AcceptThreadAvr[Count] = _Monitor_AcceptSocket;
		_Monitor_SendThreadAvr[Count] = _Monitor_Counter_PacketSend;
		_Monitor_AuthThreadAvr[Count] = _Monitor_Counter_AuthUpdate;
		_Monitor_GameThreadAvr[Count++]= _Monitor_Counter_GameUpdate;
		

		if (5 == Count)
			Count = 0;
		for (int i = 0; i < 5; i++)
		{
//			_Monitor_Counter_RecvAvr += _Monitor_RecvAvr[i];
//			_Monitor_Counter_SendAvr += _Monitor_SendAvr[i];
			_Monitor_Counter_AcceptThreadAvr +=	_Monitor_AcceptThreadAvr[i];
			_Monitor_Counter_SendThreadAvr += _Monitor_SendThreadAvr[i];
			_Monitor_Counter_AuthThreadAvr += _Monitor_AuthThreadAvr[i];
			_Monitor_Counter_GameThreadAvr += _Monitor_GameThreadAvr[i];
			_Monitor_Counter_NetworkRecvAvr += _Monitor_NetworkRecvBytes[i];
			_Monitor_Counter_NetworkSendAvr += _Monitor_NetworkSendBytes[i];
		}
//		_Monitor_Counter_RecvAvr = _Monitor_Counter_RecvAvr / 5;
//		_Monitor_Counter_SendAvr = _Monitor_Counter_SendAvr / 5;
		_Monitor_Counter_AcceptThreadAvr = _Monitor_Counter_AcceptThreadAvr / 5;
		_Monitor_Counter_SendThreadAvr = _Monitor_Counter_SendThreadAvr / 5;
		_Monitor_Counter_AuthThreadAvr = _Monitor_Counter_AuthThreadAvr / 5;
		_Monitor_Counter_GameThreadAvr = _Monitor_Counter_GameThreadAvr / 5;
		_Monitor_Counter_NetworkRecvAvr = _Monitor_Counter_NetworkRecvAvr / 5;
		_Monitor_Counter_NetworkSendAvr = _Monitor_Counter_NetworkSendAvr / 5;

//		_Monitor_Counter_Recv = 0;
//		_Monitor_Counter_Send = 0;
		_Monitor_AcceptSocket = 0;
		_Monitor_Counter_PacketSend = 0;
		_Monitor_Counter_AuthUpdate = 0;
		_Monitor_Counter_GameUpdate = 0;
		_Ethernet._pdh_value_Network_RecvBytes = 0;
		_Ethernet._pdh_value_Network_SendBytes = 0;
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
		PdhGetFormattedCounterValue(_MemoryNonpagedBytes, PDH_FMT_DOUBLE, NULL, &_CounterVal);
		_Nonpaged_Memory = (int)_CounterVal.doubleValue / (1024 * 1024);
		PdhGetFormattedCounterValue(_MemoryAvailableMBytes, PDH_FMT_DOUBLE, NULL, &_CounterVal);
		_Available_Memory = (int)_CounterVal.doubleValue;
		PdhGetFormattedCounterValue(_ProcessPrivateBytes, PDH_FMT_DOUBLE, NULL, &_CounterVal);
		_BattleServer_Memory_Commit = (int)_CounterVal.doubleValue / (1024 * 1024);
		//	시스템 측정 쿼리 갱신

		MakePacket(dfMONITOR_DATA_TYPE_SERVER_CPU_TOTAL);
		MakePacket(dfMONITOR_DATA_TYPE_SERVER_AVAILABLE_MEMORY);
		MakePacket(dfMONITOR_DATA_TYPE_SERVER_NETWORK_RECV);
		MakePacket(dfMONITOR_DATA_TYPE_SERVER_NETWORK_SEND);
		MakePacket(dfMONITOR_DATA_TYPE_SERVER_NONPAGED_MEMORY);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_SERVER_ON);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_CPU);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_MEMORY_COMMIT);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_PACKET_POOL);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_AUTH_FPS);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_GAME_FPS);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_SESSION_ALL);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_SESSION_AUTH);
		MakePacket(dfMONITOR_DATA_TYPE_BATTLE_SESSION_GAME);
		MakePacket(dfMONITOR_DATA_TYPE_CHAT_SERVER_ON);
		MakePacket(dfMONITOR_DATA_TYPE_MATCH_SERVER_ON);
	}
	return true;
}

bool CGameServer::MakePacket(BYTE DataType)
{
	struct tm *pTime = new struct tm;
	time_t Now;
	localtime_s(pTime, &Now);
	_TimeStamp = time(NULL);
	WORD Type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	switch (DataType)
	{
	case dfMONITOR_DATA_TYPE_SERVER_CPU_TOTAL:
	{
		_CPU_Total = _Cpu.ProcessorTotal();
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _CPU_Total << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_SERVER_AVAILABLE_MEMORY:
	{
		//	사용가능 메모리 얻기
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _Available_Memory << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_SERVER_NETWORK_RECV:
	{
		_Network_Recv = _Monitor_Counter_NetworkRecvAvr;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _Network_Recv << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_SERVER_NETWORK_SEND:
	{
		_Network_Send = _Monitor_Counter_NetworkSendAvr;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _Network_Send << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_SERVER_NONPAGED_MEMORY:
	{
		//	논페이지드 메모리 얻기
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _Nonpaged_Memory << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_SERVER_ON:
	{
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_On << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_CPU:
	{
		_BattleServer_CPU = _Cpu.ProcessTotal();
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_CPU << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_MEMORY_COMMIT:
	{
		//	메모리 커밋(private)MByte
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_Memory_Commit << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_PACKET_POOL:
	{
		_BattleServer_PacketPool = CPacket::m_pMemoryPool->_UseCount;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_PacketPool << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_AUTH_FPS:
	{
		_BattleServer_Auth_FPS = _Monitor_Counter_AuthThreadAvr;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_Auth_FPS << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_GAME_FPS:
	{
		_BattleServer_Game_FPS = _Monitor_Counter_GameThreadAvr;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_Game_FPS << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_SESSION_ALL:
	{
		_BattleServer_Session_ALL = _Monitor_SessionAllMode;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_Session_ALL << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_SESSION_AUTH:
	{
		_BattleServer_Session_Auth = _Monitor_SessionAuthMode;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_Session_Auth << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	case dfMONITOR_DATA_TYPE_BATTLE_SESSION_GAME:
	{
		_BattleServer_Session_Game = _Monitor_SessionGameMode;
		CPacket *pPacket = CPacket::Alloc();
		*pPacket << Type << DataType << _BattleServer_Session_Game << _TimeStamp;
		_pMonitor->SendPacket(pPacket);
		pPacket->Free();
	}
	break;
	default:
		break;
	}
	delete pTime;
	return true;
}









































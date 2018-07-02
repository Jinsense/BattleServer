#ifndef _BATTLESERVER_SERVER_GAMESERVER_H_
#define _BATTLESERVER_SERVER_GAMESERVER_H_

#include <Pdh.h>

#include "CommonProtocol.h"
#include "CpuUsage.h"
#include "EtherNet_PDH.h"
#include "LanClient.h"
#include "BattleServer.h"

#pragma comment(lib, "Pdh.lib")

class CGameServer : public CBattleServer
{
public:
	CGameServer(int iMaxSession, int iSend, int iAuth, int iGame);
	~CGameServer();

	void OnConnectionRequest();
	void OnAuth_Update();
	void OnGame_Update();
	void OnError(int iErrorCode, WCHAR *szError);

	bool MonitorInit();
	bool MonitorOnOff();
	static unsigned int __stdcall MonitorThread(void *pParam)
	{
		CGameServer *pMonitorThread = (CGameServer*)pParam;
		if (NULL == pMonitorThread)
		{
			wprintf(L"[GameServer :: MonitorThread] Init Error\n");
			return false;
		}
		pMonitorThread->MonitorThread_update();
		return true;
	}
	bool MonitorThread_update();

	static unsigned int __stdcall LanMonitorThread(void *pParam)
	{
		CGameServer *pLanMonitorThread = (CGameServer*)pParam;
		if (NULL == pLanMonitorThread)
		{
			wprintf(L"[GameServer :: LanMonitorThread] Init Error\n");
			return false;
		}
		pLanMonitorThread->LanMonitorThread_Update();
		return true;
	}
	bool LanMonitorThread_Update();
	bool MakePacket(BYTE DataType);

public:
	CLanClient	*_pMonitor;

	int		_TimeStamp;						//	TimeStamp
	int		_CPU_Total;						//	CPU 전체 사용율
	int		_Available_Memory;				//	사용가능한 메모리
	int		_Network_Recv;					//	하드웨어 이더넷 수신
	int		_Network_Send;					//	하드웨어 이더넷 송신
	int		_Nonpaged_Memory;				//	논페이지드 메모리

	int		_BattleServer_On;				//	배틀서버 ON / OFF
	int		_BattleServer_CPU;				//	배틀서버 CPU
	int		_BattleServer_Memory_Commit;	//	배틀서버 메모리 커밋 (Private) MByte
	int		_BattleServer_PacketPool;		//	배틀서버 패킷풀 사용량
	int		_BattleServer_Auth_FPS;			//	배틀서버 Auth FPS
	int		_BattleServer_Game_FPS;			//	배틀서버 Game FPS
	int		_BattleServer_Session_ALL;		//	배틀서버 세션 전체 접속자 수
	int		_BattleServer_Session_Auth;		//	배틀서버 Auth 모드 접속자 수
	int		_BattleServer_Session_Game;		//	배틀서버 Game 모드 접속자 수

private:
	bool	_bMonitor;
	CPlayer *_pPlayer;
	CCpuUsage _Cpu;
	CEthernet _Ethernet;
	HANDLE	_hMonitorThread;
	HANDLE	_hLanMonitorThread;

	PDH_HQUERY		_CpuQuery;
	PDH_HCOUNTER	_MemoryAvailableMBytes;
	PDH_HCOUNTER	_MemoryNonpagedBytes;
	PDH_HCOUNTER	_ProcessPrivateBytes;
	PDH_FMT_COUNTERVALUE _CounterVal;

};

#endif

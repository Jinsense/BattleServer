#ifndef _BATTLESERVER_SERVER_GAMESERVER_H_
#define _BATTLESERVER_SERVER_GAMESERVER_H_

#include <Pdh.h>
#include <iostream>
#include <string>
#include <time.h>
#include <list>

#include "define.h"
#include "CommonProtocol.h"
#include "CpuUsage.h"
#include "EtherNet_PDH.h"
#include "BattleServer.h"
#include "LanMasterClient.h"
#include "LanMonitorClient.h"

#pragma comment(lib, "Pdh.lib")

using namespace std;

class CGameServer : public CBattleServer
{
public:
	CGameServer();
	CGameServer(int iMaxSession, int iSend, int iAuth, int iGame);
	~CGameServer();

	//-----------------------------------------------------------
	//	가상함수
	//-----------------------------------------------------------
	void	OnConnectionRequest();
	void	OnAuth_Update();
	void	OnGame_Update();
	void	OnError(int iErrorCode, WCHAR *szError);

	//-----------------------------------------------------------
	//	모니터링 관련 스레드 / 함수
	//-----------------------------------------------------------
	bool	ThreadInit();
	void	MonitorSendPacket();
	bool	MonitorSendPacketCreate(BYTE DataType);
	bool	MonitorOnOff();
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
	bool	MonitorThread_update();

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
	bool	LanMonitorThread_Update();
	//-----------------------------------------------------------
	//	마스터 서버 연결 체크
	//-----------------------------------------------------------
	static unsigned int WINAPI LanMasterCheckThread(LPVOID arg)
	{
		CGameServer *_pLanMasterCheckThread = (CGameServer *)arg;
		if (NULL == _pLanMasterCheckThread)
		{
			std::wprintf(L"[Server :: LanMonitoringThread] Init Error\n");
			return false;
		}
		_pLanMasterCheckThread->LanMasterCheckThead_Update();
		return true;
	}
	void	LanMasterCheckThead_Update();
	//-----------------------------------------------------------
	//	Http 통신 담당 스레드
	//-----------------------------------------------------------
	static unsigned int WINAPI HttpSendThread(LPVOID arg)
	{
		CGameServer *_pHttpSendThread = (CGameServer *)arg;
		if (NULL == _pHttpSendThread)
		{
			std::wprintf(L"[Server :: _pHttpSendThread] Init Error\n");
			return false;
		}
		_pHttpSendThread->HttpSendThread_Update();
		return true;
	}
	void	HttpSendThread_Update();
	void	HttpSend_Select(CRingBuffer * pBuffer);
	void	HttpSend_Update(CRingBuffer * pBuffer);	
	string	HttpPacket_Create(Json::Value PostData);
	//-----------------------------------------------------------
	//	사용자 함수 - OnAuth_Update
	//-----------------------------------------------------------
	void	WaitRoomSizeCheck();
	void	WaitRoomCreate();
	void	WaitRoomReadyCheck();
	void	WaitRoomGameReadyCheck();
	//-----------------------------------------------------------
	//	사용자 함수 - OnGame_Update
	//-----------------------------------------------------------
	void	PlayRoomGameEndCheck();
	void	PlayRoomDestroyCheck();

private:
	void	EntertokenCreate(char *pBuff);
	void	NewConnectTokenCreate();

public:
	CPlayer * _pPlayer;

	std::map<int, BATTLEROOM*> _WaitRoomMap;
	std::map<int, BATTLEROOM*> _PlayRoomMap;
	std::map<int, BATTLEROOM*> _TempMap;
	std::list<int> _ClosedRoomlist;
	SRWLOCK		_WaitRoom_lock;
	SRWLOCK		_PlayRoom_lock;
	SRWLOCK		_ClosedRoom_lock;
	SRWLOCK		_RoomPlayer_lock;
	CMemoryPool<BATTLEROOM> *_BattleRoomPool;
	CMemoryPool<RoomPlayerInfo> *_RoomPlayerPool;
	CMemoryPool<CRingBuffer> *_HttpPool;
	CLockFreeQueue<CRingBuffer*> _HttpQueue;
	HANDLE	_hHttpEvent;

	char	_OldConnectToken[32];			//	배틀서버 접속 토큰 ( 기존 )
	char	_CurConnectToken[32];			//	배틀서버 접속 토큰 ( 신규 )
	__int64 _CreateTokenTick;				//	토큰 신규 발행한 시간

	CLanMonitorClient * _pMonitor;
	CLanMasterClient *	_pMaster;

	UINT	_Sequence;
	int		_RoomCnt;
	long	_WaitRoomCount;
	long	_CountDownRoomCount;
	long	_PlayRoomCount;
	int		_BattleServerNo;

	int		_TimeStamp;						//	TimeStamp	

	int		_BattleServer_On;				//	배틀서버 ON / OFF
	int		_BattleServer_CPU;				//	배틀서버 CPU ( 커널 + 유저 )
	int		_BattleServer_Memory_Commit;	//	배틀서버 메모리 커밋 (Private) MByte
	int		_BattleServer_PacketPool;		//	배틀서버 패킷풀 사용량
	int		_BattleServer_Auth_FPS;			//	배틀서버 Auth FPS
	int		_BattleServer_Game_FPS;			//	배틀서버 Game FPS
	int		_BattleServer_Session_ALL;		//	배틀서버 세션 전체 접속자 수
	int		_BattleServer_Session_Auth;		//	배틀서버 Auth 모드 접속자 수
	int		_BattleServer_Session_Game;		//	배틀서버 Game 모드 접속자 수
	int		_BattleServer_Session_Login;	//	배틀서버 로그인 성공 전체 인원 - 삭제
	int		_BattleServer_Room_Wait;		//	배틀서버 대기방 수
	int		_BattleServer_Room_Play;		//	배틀서버 플레이방 수

private:
	bool	_bMonitor;
	CCpuUsage _Cpu;
	CEthernet _Ethernet;
	HANDLE	_hMonitorThread;
	HANDLE	_hLanMonitorThread;
	HANDLE	_hLanMasterCheckThread;
	HANDLE	_hHttpSendThread[10];

	PDH_HQUERY		_CpuQuery;
	PDH_HCOUNTER	_MemoryAvailableMBytes;
	PDH_HCOUNTER	_MemoryNonpagedBytes;
	PDH_HCOUNTER	_ProcessPrivateBytes;
	PDH_FMT_COUNTERVALUE _CounterVal;

};

#endif _BATTLESERVER_SERVER_GAMESERVER_H_

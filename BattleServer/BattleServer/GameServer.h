#ifndef _BATTLESERVER_SERVER_GAMESERVER_H_
#define _BATTLESERVER_SERVER_GAMESERVER_H_

#include <Pdh.h>
#include <iostream>
#include <string>
#include <time.h>

#include "CommonProtocol.h"
#include "CpuUsage.h"
#include "EtherNet_PDH.h"
#include "BattleServer.h"
#include "LanClient.h"

#pragma comment(lib, "Pdh.lib")

using namespace std;

enum enENTERROOM_RESULT
{
	SUCCESS = 1,
	ENTERTOKEN_FAIL = 2,
	NOT_READYROOM = 3,
	NOT_FINDROOM = 4,
	ROOMUSER_MAX = 5,
};

enum enHTTPTYPE
{
	SELECT = 1,
	UPDATE = 2,

};
typedef struct st_RoomPlayerInfo
{
	UINT64 AccountNo;
	int  Index;
}RoomPlayerInfo;

typedef struct st_BattleRoom
{
	char Entertoken[32] = { 0, };	//	Entertoken;
	int RoomNo;			//	방 번호
	int MaxUser;		//	최대 유저
	int CurUser;		//	현재 유저
	std::vector<RoomPlayerInfo> RoomPlayer;		//	방에 있는 유저 목록
	__int64 ReadyCount;	//	대기방 준비완료 시간
	bool RoomReady;		//	대기방 준비완료 플래그
	bool PlayReady;		//	게임준비 완료 플래그
}BATTLEROOM;

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
	void	OnRoomLeavePlayer(int RoomNo, INT64 AccountNo);

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
	

private:
	void	EntertokenCreate(char *pBuff);
	void	NewConnectTokenCreate();

public:
	std::map<int, BATTLEROOM*> _WaitRoomMap;
	std::map<int, BATTLEROOM*> _PlayRoomMap;
	SRWLOCK		_WaitRoom_lock;
	SRWLOCK		_PlayRoom_lock;
	CMemoryPool<BATTLEROOM> *_BattleRoomPool;
	CMemoryPool<CRingBuffer> *_HttpPool;
	CLockFreeQueue<CRingBuffer*> _HttpQueue;
	HANDLE	_hHttpEvent;

	char	_OldConnectToken[32];			//	배틀서버 접속 토큰 ( 기존 )
	char	_CurConnectToken[32];			//	배틀서버 접속 토큰 ( 신규 )
	__int64 _CreateTokenTick;				//	토큰 신규 발행한 시간

	CLanClient * _pMonitor;
	CLanClient * _pMaster;

	int		_RoomCnt;
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
	CPlayer *_pPlayer;
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

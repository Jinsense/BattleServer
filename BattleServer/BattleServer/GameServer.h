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
	int RoomNo;			//	�� ��ȣ
	int MaxUser;		//	�ִ� ����
	int CurUser;		//	���� ����
	std::vector<RoomPlayerInfo> RoomPlayer;		//	�濡 �ִ� ���� ���
	__int64 ReadyCount;	//	���� �غ�Ϸ� �ð�
	bool RoomReady;		//	���� �غ�Ϸ� �÷���
	bool PlayReady;		//	�����غ� �Ϸ� �÷���
}BATTLEROOM;

class CGameServer : public CBattleServer
{
public:
	CGameServer();
	CGameServer(int iMaxSession, int iSend, int iAuth, int iGame);
	~CGameServer();

	//-----------------------------------------------------------
	//	�����Լ�
	//-----------------------------------------------------------
	void	OnConnectionRequest();
	void	OnAuth_Update();
	void	OnGame_Update();
	void	OnError(int iErrorCode, WCHAR *szError);
	void	OnRoomLeavePlayer(int RoomNo, INT64 AccountNo);

	//-----------------------------------------------------------
	//	����͸� ���� ������ / �Լ�
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
	//	������ ���� ���� üũ
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
	//	Http ��� ��� ������
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
	//	����� �Լ� - OnAuth_Update
	//-----------------------------------------------------------
	void	WaitRoomSizeCheck();
	void	WaitRoomCreate();
	void	WaitRoomReadyCheck();
	void	WaitRoomGameReadyCheck();
	//-----------------------------------------------------------
	//	����� �Լ� - OnGame_Update
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

	char	_OldConnectToken[32];			//	��Ʋ���� ���� ��ū ( ���� )
	char	_CurConnectToken[32];			//	��Ʋ���� ���� ��ū ( �ű� )
	__int64 _CreateTokenTick;				//	��ū �ű� ������ �ð�

	CLanClient * _pMonitor;
	CLanClient * _pMaster;

	int		_RoomCnt;
	int		_BattleServerNo;

	int		_TimeStamp;						//	TimeStamp	

	int		_BattleServer_On;				//	��Ʋ���� ON / OFF
	int		_BattleServer_CPU;				//	��Ʋ���� CPU ( Ŀ�� + ���� )
	int		_BattleServer_Memory_Commit;	//	��Ʋ���� �޸� Ŀ�� (Private) MByte
	int		_BattleServer_PacketPool;		//	��Ʋ���� ��ŶǮ ��뷮
	int		_BattleServer_Auth_FPS;			//	��Ʋ���� Auth FPS
	int		_BattleServer_Game_FPS;			//	��Ʋ���� Game FPS
	int		_BattleServer_Session_ALL;		//	��Ʋ���� ���� ��ü ������ ��
	int		_BattleServer_Session_Auth;		//	��Ʋ���� Auth ��� ������ ��
	int		_BattleServer_Session_Game;		//	��Ʋ���� Game ��� ������ ��
	int		_BattleServer_Session_Login;	//	��Ʋ���� �α��� ���� ��ü �ο� - ����
	int		_BattleServer_Room_Wait;		//	��Ʋ���� ���� ��
	int		_BattleServer_Room_Play;		//	��Ʋ���� �÷��̹� ��

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

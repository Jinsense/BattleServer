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
	//	�����Լ�
	//-----------------------------------------------------------
	void	OnConnectionRequest();
	void	OnAuth_Update();
	void	OnGame_Update();
	void	OnError(int iErrorCode, WCHAR *szError);

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

	char	_OldConnectToken[32];			//	��Ʋ���� ���� ��ū ( ���� )
	char	_CurConnectToken[32];			//	��Ʋ���� ���� ��ū ( �ű� )
	__int64 _CreateTokenTick;				//	��ū �ű� ������ �ð�

	CLanMonitorClient * _pMonitor;
	CLanMasterClient *	_pMaster;

	UINT	_Sequence;
	int		_RoomCnt;
	long	_WaitRoomCount;
	long	_CountDownRoomCount;
	long	_PlayRoomCount;
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

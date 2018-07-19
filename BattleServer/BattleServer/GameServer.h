#ifndef _BATTLESERVER_SERVER_GAMESERVER_H_
#define _BATTLESERVER_SERVER_GAMESERVER_H_

#include <Pdh.h>
#include <iostream>
#include <string>
#include <time.h>

#include "CommonProtocol.h"
#include "CpuUsage.h"
#include "EtherNet_PDH.h"
#include "LanClient.h"
#include "BattleServer.h"

#pragma comment(lib, "Pdh.lib")

using namespace std;

class CGameServer : public CBattleServer
{
public:
	CGameServer(int iMaxSession, int iSend, int iAuth, int iGame);
	~CGameServer();

	void OnConnectionRequest();
	void OnAuth_Update();
	void OnGame_Update();
	void OnError(int iErrorCode, WCHAR *szError);
	void OnRoomLeavePlayer(int RoomNo, INT64 AccountNo);

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

	bool	LanMonitorThread_Update();
	bool	MakePacket(BYTE DataType);
	void	LanMasterCheckThead_Update();

private:
	void NewConnectTokenCreate();

public:
	CLanClient	*_pMonitor;
	CLanClient	*_pMaster;

	int		_RoomCnt;
	int		_BattleServerNo;

	int		_TimeStamp;						//	TimeStamp
	int		_CPU_Total;						//	CPU ��ü �����
	int		_Available_Memory;				//	��밡���� �޸�
	int		_Network_Recv;					//	�ϵ���� �̴��� ����
	int		_Network_Send;					//	�ϵ���� �̴��� �۽�
	int		_Nonpaged_Memory;				//	���������� �޸�

	int		_BattleServer_On;				//	��Ʋ���� ON / OFF
	int		_BattleServer_CPU;				//	��Ʋ���� CPU
	int		_BattleServer_Memory_Commit;	//	��Ʋ���� �޸� Ŀ�� (Private) MByte
	int		_BattleServer_PacketPool;		//	��Ʋ���� ��ŶǮ ��뷮
	int		_BattleServer_Auth_FPS;			//	��Ʋ���� Auth FPS
	int		_BattleServer_Game_FPS;			//	��Ʋ���� Game FPS
	int		_BattleServer_Session_ALL;		//	��Ʋ���� ���� ��ü ������ ��
	int		_BattleServer_Session_Auth;		//	��Ʋ���� Auth ��� ������ ��
	int		_BattleServer_Session_Game;		//	��Ʋ���� Game ��� ������ ��

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

#endif _BATTLESERVER_SERVER_GAMESERVER_H_

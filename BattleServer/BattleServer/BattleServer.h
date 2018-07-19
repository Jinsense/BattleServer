#ifndef _BATTLESERVER_SERVER_BATTLESERVER_H_
#define _BATTLESERVER_SERVER_BATTLESERVER_H_

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include <map>

#include "Packet.h"
#include "RingBuffer.h"
#include "MemoryPool.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "Player.h"
#include "Log.h"
#include "Dump.h"

#define		WORKER_THREAD_MAX		10
#define		WSABUF_MAX				100
#define		RELEASE_MAX				100

extern CConfig Config;

enum enENTERROOM_RESULT
{
	SUCCESS = 1,
	ENTERTOKEN_FAIL = 2,
	NOT_READYROOM = 3,
	NOT_FINDROOM = 4,
	ROOMUSER_MAX = 5,
};

typedef struct st_RoomPlayerInfo
{
	UINT64 AccountNo;
	int  Index;
}RoomPlayerInfo;

typedef struct st_BattleRoom
{
	int RoomNo;			//	�� ��ȣ
	int MaxUser;		//	�ִ� ����
	int CurUser;		//	���� ����
	std::vector<RoomPlayerInfo> RoomPlayer;		//	�濡 �ִ� ���� ���
	__int64 ReadyCount;	//	���� �غ�Ϸ� �ð�
	bool RoomReady;		//	���� �غ�Ϸ� �÷���
	bool GameReady;		//	�����غ� �Ϸ� �÷���
	bool GameStart;     //  ���Ӹ�� ��ȯ ����
}BATTLEROOM;

class CBattleServer
{
public:
	CBattleServer(int iMaxSession, int iSend, int iAuth, int iGame);
	virtual ~CBattleServer();

	bool Start(WCHAR *szListenIP, int iPort, int iWorkerThread, bool bEnableNagle, BYTE byPacketCode, BYTE byPacketKey1, BYTE byPacketKey2);
	bool Stop();

	//	�ܺο��� ���ǰ�ü ����
	void SetSessionArray(int iArrayIndex, CNetSession *pSession);

	//	���� ������ ����
	bool CreateThread();

	//	Socket IOCP ���
	bool CreateIOCP_Socket(SOCKET Socket, ULONG_PTR Key);

	//	��Ŷ ������, ��ü���� ���
	void SendPacket_GameAll(CPacket *pPacket, unsigned __int64 ExcludeClientID);

	//	��Ŷ ������, Ư�� Ŭ���̾�Ʈ
	void SendPacket(CPacket *pPacket, unsigned __int64 ClientID);

	bool SessionAcquireLock(int Index);
	bool SessionAcquireFree(int Index);
private:
	void Error(int ErrorCode, WCHAR *szFormatStr, ...);
	void StartRecvPost(int Index);
	void RecvPost(int Index);
	void SendPost(int Index);
	void CompleteRecv(int Index, DWORD Trans);
	void CompleteSend(int Index, DWORD Trans);

	//	Auth, Game �������� ó���Լ�
	void ProcAuth_Accept();
	void ProcAuth_LogoutInAuth();
	void ProcAuth_Logout();
	void ProcAuth_AuthToGame();

	void ProcGame_AuthToGame();
	void ProcGame_LogoutInGame();
	void ProcGame_Logout();
	void ProcGame_Release();

	//	������ �Լ�
	static unsigned int __stdcall	AcceptThread(void *pParam)
	{
		CBattleServer *pAcceptThread = (CBattleServer*)pParam;
		if (NULL == pAcceptThread)
		{
			wprintf(L"[MMOServer :: AcceptThread] Init Error\n");
			return false;
		}
		pAcceptThread->AcceptThread_update();
		return true;
	}

	bool AcceptThread_update();

	static unsigned int __stdcall	AuthThread(void *pParam)
	{
		CBattleServer *pAuthThread = (CBattleServer*)pParam;
		if (NULL == pAuthThread)
		{
			wprintf(L"[MMOServer :: AuthThread] Init Error\n");
			return false;
		}
		pAuthThread->AuthThread_update();
		return true;
	}
	bool AuthThread_update();

	static unsigned __stdcall	GameUpdateThread(void *pParam)
	{
		CBattleServer *pGameUpdateThread = (CBattleServer*)pParam;
		if (NULL == pGameUpdateThread)
		{
			wprintf(L"[MMOServer :: GameUpdateThread] Init Error\n");
			return false;
		}
		pGameUpdateThread->GameUpdateThread_update();
		return true;
	}
	bool GameUpdateThread_update();


	static unsigned __stdcall	IOCPWorkerThread(void *pParam)
	{
		CBattleServer *pIOCPWorkerThread = (CBattleServer*)pParam;
		if (NULL == pIOCPWorkerThread)
		{
			wprintf(L"[MMOServer :: IOCPWorkerThread] Init Error\n");
			return false;
		}
		pIOCPWorkerThread->IOCPWorkerThread_update();
		return true;
	}
	bool IOCPWorkerThread_update();

	static unsigned __stdcall	SendThread(void *pParam)
	{
		CBattleServer *pSendThread = (CBattleServer*)pParam;
		if (NULL == pSendThread)
		{
			wprintf(L"[MMOServer :: SendThread] Init Error\n");
			return false;
		}
		pSendThread->SendThread_update();
		return true;
	}
	bool SendThread_update();

private:
	virtual void OnConnectionRequest() = 0;
	
	//	AUTH ��� ������Ʈ �̺�Ʈ ����ó����
	virtual void OnAuth_Update() = 0;
	
	//	GAME ��� ������Ʈ �̺�Ʈ ����ó����
	virtual void OnGame_Update() = 0;

	virtual void OnError(int iErrorCode, WCHAR *szError) = 0;
	virtual void OnRoomLeavePlayer(int RoomNo, INT64 AccountNo) = 0;

public:
	const int _iMaxSession;
	const int _iSendThread;
	const int _iAuthThread;
	const int _iGameThread;
	
protected:
	bool _bShutdown;
	bool _bShutdownListen;

private:
	SOCKET _ListenSocket;

	BYTE _byCode;
	bool _bEnableNagle;
	int _iWorkerThread;

	WCHAR _szListenIP[16];
	int _iListenPort;
	unsigned __int64 _iClientIDCnt;

	HANDLE _hAcceptThread;

	CRingBuffer		_AccpetSocketQueue;
//	CLockFreeQueue<CLIENT_CONNECT_INFO *>	_AccpetSocketQueue;	//	�ű����� Socket ť
	CMemoryPool<CLIENT_CONNECT_INFO>		*_pMemoryPool_ConnectInfo;

	//	Auth ��
	HANDLE	_hAuthThread;

	CLockFreeStack<int>	_BlankSessionStack;		//	�� ���� index

	//	GameUpdate ��
	HANDLE	_hGameUpdateThread;

	//	IOCP ��
	HANDLE	_hIOCPWorkerThread[WORKER_THREAD_MAX];
	HANDLE	_hIOCP;

	//	Send ��
	HANDLE	_hSendThread;
public:
	std::map<int, BATTLEROOM*> _BattleRoomMap;
	SRWLOCK		_BattleRoom_lock;
	CMemoryPool<BATTLEROOM> *_BattleRoomPool;
	char	_OldConnectToken[32];			//	��Ʋ���� ���� ��ū ( ���� )
	char	_CurConnectToken[32];			//	��Ʋ���� ���� ��ū ( �ű� )
	__int64 _CreateTokenTick;				//	��ū �ű� ������ �ð�

	CNetSession	**_pSessionArray;
	SRWLOCK		_Srwlock;

	long long	_Monitor_AcceptTotal;			//	Accept �� Ƚ��
	long		_Monitor_AcceptSocket;			//	1�� �� Accept Ƚ��
	long		_Monitor_SessionAllMode;		//	��ü ������ ��
	long		_Monitor_SessionAuthMode;		//	AuthMode ������ ��
	long		_Monitor_SessionGameMode;		//	GameMode ������ ��

	long		_Monitor_Counter_AuthUpdate;	//	1�� �� AuthThread ���� Ƚ��
	long		_Monitor_Counter_GameUpdate;	//	1�� �� GameThread ���� Ƚ��
	long		_Monitor_Counter_Recv;			//	1�� �� Recv ���� Ƚ��
	long		_Monitor_Counter_Send;			//	1�� �� Send ���� Ƚ��
	long		_Monitor_Counter_PacketSend;	//	1�� �� SendThread ���� Ƚ��

	long		_Monitor_Counter_RecvAvr;
	long		_Monitor_Counter_SendAvr;
	long		_Monitor_Counter_AcceptThreadAvr;
	long		_Monitor_Counter_SendThreadAvr;
	long		_Monitor_Counter_AuthThreadAvr;
	long		_Monitor_Counter_GameThreadAvr;
	long		_Monitor_Counter_NetworkRecvAvr;
	long		_Monitor_Counter_NetworkSendAvr;

	long		_Monitor_RecvAvr[100] = { 0, };
	long		_Monitor_SendAvr[100] = { 0, };
	long		_Monitor_AcceptThreadAvr[100] = { 0, };
	long		_Monitor_SendThreadAvr[100] = { 0, };
	long		_Monitor_AuthThreadAvr[100] = { 0, };
	long		_Monitor_GameThreadAvr[100] = { 0, };
	long		_Monitor_NetworkRecvBytes[100] = { 0, };
	long		_Monitor_NetworkSendBytes[100] = { 0, };

	CSystemLog	*_pLog;
	std::map<INT64, CNetSession*>	_AccountNoMap;
	SRWLOCK				_AccountNoMap_srwlock;
};

#endif

#ifndef _BATTLE_SERVER_LANSERVER_H_
#define _BATTLE_SERVER_LANSERVER_H_

#include <map>
#include <list>
#include <vector>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include "Packet.h"
#include "RingBuffer.h"
#include "MemoryPool.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"


#define		LAN_WSABUF_NUMBER		100
#define		LAN_QUEUE_SIZE			10000
#define		LAN_HEADER_SIZE			2
#define		LAN_MONITOR_NUM			42
#define		LAN_SERVER_NUM			10

#define		SET_INDEX(Index, SessionKey)		Index = Index << 48; SessionKey = Index | SessionKey;
#define		GET_INDEX(Index, SessionKey)		Index = SessionKey >> 48;

typedef struct st_MONITOR
{
	BYTE Type;
	int Value;
	int TimeStamp;
	int Min;
	int Max;
	float Avr;
	bool Recv;
	st_MONITOR()
	{
		Type = NULL, Value = NULL, TimeStamp = NULL, Min = 0xffffffff, Max = 0, Avr = NULL, Recv = false;
	}
}MONITOR;

typedef struct st_ServerInfo
{
	bool Login;
	char IP[20];
	USHORT Port;
	st_ServerInfo()
	{
		Login = false, ZeroMemory(&IP, sizeof(IP)), Port = NULL;
	}
}SERVERINFO;

typedef struct st_RELEASE_COMPARE
{
	__int64	iIOCount;
	__int64	iReleaseFlag;

	st_RELEASE_COMPARE() :
		iIOCount(0),
		iReleaseFlag(false) {}
}LANCOMPARE;

typedef struct st_Client
{
	int					ServerNo;
	char				IP[20];
	unsigned short		Port;
	bool				bLoginFlag;
	bool				bRelease;
	int					Index;
	long				lIOCount;
	long				lSendFlag;
	long				lSendCount;
	unsigned __int64	iClientID;
	SOCKET				sock;
	OVERLAPPED			SendOver;
	OVERLAPPED			RecvOver;
	CRingBuffer			RecvQ;
	CRingBuffer			PacketQ;
	CLockFreeQueue<CPacket*> SendQ;
	st_Client() :
		RecvQ(LAN_QUEUE_SIZE),
		PacketQ(LAN_QUEUE_SIZE),
		lIOCount(0),
		lSendFlag(true) {}
}LANSESSION;

class CGameServer;

class CChatLanServer
{
public:
	CChatLanServer();
	~CChatLanServer();

	void				Disconnect(unsigned __int64 iClientID);
	/*virtual void		OnClientJoin(st_SessionInfo *pInfo) = 0;
	virtual void		OnClientLeave(unsigned __int64 iClientID) = 0;
	virtual void		OnConnectionRequest(WCHAR * pClientIP, int iPort) = 0;
	virtual void		OnError(int iErrorCode, WCHAR *pError) = 0;*/
	unsigned __int64	GetClientCount();

	bool				Set(CGameServer *pGameServer);
	bool				ServerStart(WCHAR *pOpenIP, int iPort, int iMaxWorkerThread,
		bool bNodelay, int iMaxSession);
	bool				ServerStop();
	bool				SendPacket(unsigned __int64 iClientID, CPacket *pPacket);
	bool				GetShutDownMode() { return _bShutdown; }
	bool				GetWhiteIPMode() { return _bWhiteIPMode; }
	bool				SetShutDownMode(bool bFlag);
	bool				SetWhiteIPMode(bool bFlag);

	LANSESSION*			SessionAcquireLock(unsigned __int64 iClientID);
	void				SessionAcquireFree(LANSESSION *pSession);

private:
	bool				ServerInit();
	bool				ClientShutdown(LANSESSION *pSession);
	bool				ClientRelease(LANSESSION *pSession);

	static unsigned int WINAPI WorkerThread(LPVOID arg)
	{
		CChatLanServer *_pWorkerThread = (CChatLanServer *)arg;
		if (_pWorkerThread == NULL)
		{
			wprintf(L"[Server :: WorkerThread]	Init Error\n");
			return false;
		}
		_pWorkerThread->WorkerThread_Update();
		return true;
	}

	static unsigned int WINAPI AcceptThread(LPVOID arg)
	{
		CChatLanServer *_pAcceptThread = (CChatLanServer*)arg;
		if (_pAcceptThread == NULL)
		{
			wprintf(L"[Server :: AcceptThread]	Init Error\n");
			return false;
		}
		_pAcceptThread->AcceptThread_Update();
		return true;
	}

	void	WorkerThread_Update();
	void	AcceptThread_Update();
	void	StartRecvPost(LANSESSION *pSession);
	void	RecvPost(LANSESSION *pSession);
	void	SendPost(LANSESSION *pSession);
	void	CompleteRecv(LANSESSION *pSession, DWORD dwTransfered);
	void	CompleteSend(LANSESSION *pSession, DWORD dwTransfered);
	bool	OnRecv(LANSESSION *pSession, CPacket *pPacket);
	unsigned __int64*	GetIndex();
	void	PutIndex(unsigned __int64 iIndex);

	//-----------------------------------------------------------
	// 패킷처리 함수
	//-----------------------------------------------------------
	void	ResChatServerOn(CPacket * pPacket, LANSESSION * pSession);
	void	ResConnectToken(CPacket * pPacket, LANSESSION * pSession);
	void	ResCreatRoom(CPacket * pPacket, LANSESSION * pSession);
	void	ResDestroyRoom(CPacket * pPacket, LANSESSION * pSession);

	//-----------------------------------------------------------
	// 사용자 함수
	//-----------------------------------------------------------
	

public:
	unsigned __int64	_iAcceptTPS;
	unsigned __int64	_iAcceptTotal;
	unsigned __int64	_iRecvPacketTPS;
	unsigned __int64	_iSendPacketTPS;
	unsigned __int64	_iConnectClient;
	unsigned __int64	_iLoginClient;

	SERVERINFO	_ServerInfo[LAN_SERVER_NUM];
private:
	CLockFreeStack<UINT64*>	_SessionStack;
	LANCOMPARE	*_pIOCompare;
	LANSESSION	*_pSessionArray;
	SOCKET		_listensock;

	HANDLE		_hIOCP;
	HANDLE		_hWorkerThread[100];
	HANDLE		_hAcceptThread;
	HANDLE		_hMonitorThread;
	HANDLE		_hAllthread[200];

	bool		_bWhiteIPMode;
	bool		_bShutdown;

	unsigned __int64	_iAllThreadCnt;
	unsigned __int64	*_pIndex;
	unsigned __int64	_iClientIDCnt;

	CGameServer		*_pGameServer;

};



#endif 
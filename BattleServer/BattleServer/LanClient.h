#ifndef _BATTLESERVER_LIB_LANCLIENT_H_
#define _BATTLESERVER_LIB_LANCLIENT_H_


#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include "Packet.h"
#include "RingBuffer.h"
#include "LockFreeQueue.h"
#include "CommonProtocol.h"

#define LANCLIENT_WORKERTHREAD	2
#define LANCLIENT_WSABUFNUM		100
#define LANCLIENT_QUEUESIZE		20000

#define LANCLIENT_HEADERSIZE	2

typedef struct st_LanClient
{
	bool		bConnect;
	long		SendFlag;
	long		Send_Count;
	long		IO_Count;
	OVERLAPPED				SendOver, RecvOver;
	CRingBuffer				RecvQ, PacketQ;
	CLockFreeQueue<CPacket*> SendQ;
	SOCKET					sock;

	st_LanClient() :
		RecvQ(LANCLIENT_QUEUESIZE),
		PacketQ(LANCLIENT_QUEUESIZE),
		SendFlag(false),
		IO_Count(0) {}
}LANCLIENTSESSION;

class CGameServer;

class CLanClient
{
public:
	CLanClient();
	~CLanClient();

	bool Connect(WCHAR * ServerIP, int Port, bool NoDelay, int MaxWorkerThread);	//   바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
	bool Disconnect();
	bool IsConnect();
	bool SendPacket(CPacket *pPacket);

	void Constructor(CGameServer *pGameServer);

	void OnEnterJoinServer();		//	서버와의 연결 성공 후
	void OnLeaveServer();			//	서버와의 연결이 끊어졌을 때

	void OnLanRecv(CPacket *pPacket);	//	하나의 패킷 수신 완료 후
	void OnLanSend(int SendSize);		//	패킷 송신 완료 후

	void OnWorkerThreadBegin();
	void OnWorkerThreadEnd();

	void OnError(int ErrorCode, WCHAR *pMsg);

private:
	static unsigned int WINAPI WorkerThread(LPVOID arg)
	{
		CLanClient * pWorkerThread = (CLanClient *)arg;
		if (pWorkerThread == NULL)
		{
			wprintf(L"[Client :: WorkerThread]	Init Error\n");
			return FALSE;
		}

		pWorkerThread->WorkerThread_Update();
		return TRUE;
	}
	void WorkerThread_Update();
	void StartRecvPost();
	void RecvPost();
	void SendPost();
	void CompleteRecv(DWORD dwTransfered);
	void CompleteSend(DWORD dwTransfered);

public:
	bool		m_Reconnect;
	long		m_iRecvPacketTPS;
	long		m_iSendPacketTPS;
	LANCLIENTSESSION	*m_Session;

private:
	HANDLE					m_hIOCP;
	HANDLE					m_hWorker_Thread[LANCLIENT_WORKERTHREAD];

	CGameServer *			_pGameServer;

};

#endif

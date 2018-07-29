#ifndef _BATTLESERVER_SERVER_LANMONITORCLIENT_H_
#define _BATTLESERVER_SERVER_LANMONITORCLIENT_H_

#include "LanClient.h"

class CGameServer;

class CLanMonitorClient
{
public:
	CLanMonitorClient();
	~CLanMonitorClient();

	bool	Connect(WCHAR * ServerIP, int Port, bool NoDelay, int MaxWorkerThread);	//   바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
	bool	Disconnect();
	bool	IsConnect();
	bool	SendPacket(CPacket *pPacket);

	void	Constructor(CGameServer *pGameServer);

	void	OnEnterJoinServer();		//	서버와의 연결 성공 후
	void	OnLeaveServer();			//	서버와의 연결이 끊어졌을 때

	void	OnLanRecv(CPacket *pPacket);	//	하나의 패킷 수신 완료 후
	void	OnLanSend(int SendSize);		//	패킷 송신 완료 후

	void	OnWorkerThreadBegin();
	void	OnWorkerThreadEnd();

	void	OnError(int ErrorCode, WCHAR *pMsg);

private:
	static unsigned int WINAPI WorkerThread(LPVOID arg)
	{
		CLanMonitorClient * pWorkerThread = (CLanMonitorClient *)arg;
		if (pWorkerThread == NULL)
		{
			wprintf(L"[Client :: WorkerThread]	Init Error\n");
			return FALSE;
		}

		pWorkerThread->WorkerThread_Update();
		return TRUE;
	}
	void	WorkerThread_Update();
	void	StartRecvPost();
	void	RecvPost();
	void	SendPost();
	void	CompleteRecv(DWORD dwTransfered);
	void	CompleteSend(DWORD dwTransfered);

public:
	bool	m_Release;
	long	m_iRecvPacketTPS;
	long	m_iSendPacketTPS;
	LANCLIENTSESSION *	m_Session;

private:
	HANDLE		m_hIOCP;
	HANDLE		m_hWorker_Thread[LANCLIENT_WORKERTHREAD];

	CGameServer *	_pGameServer;

};

#endif

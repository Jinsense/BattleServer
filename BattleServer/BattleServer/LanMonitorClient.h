#ifndef _BATTLESERVER_SERVER_LANMONITORCLIENT_H_
#define _BATTLESERVER_SERVER_LANMONITORCLIENT_H_

#include "LanClient.h"

class CGameServer;

class CLanMonitorClient
{
public:
	CLanMonitorClient();
	~CLanMonitorClient();

	bool	Connect(WCHAR * ServerIP, int Port, bool NoDelay, int MaxWorkerThread);	//   ���ε� IP, ����IP / ��Ŀ������ �� / ���ۿɼ�
	bool	Disconnect();
	bool	IsConnect();
	bool	SendPacket(CPacket *pPacket);

	void	Constructor(CGameServer *pGameServer);

	void	OnEnterJoinServer();		//	�������� ���� ���� ��
	void	OnLeaveServer();			//	�������� ������ �������� ��

	void	OnLanRecv(CPacket *pPacket);	//	�ϳ��� ��Ŷ ���� �Ϸ� ��
	void	OnLanSend(int SendSize);		//	��Ŷ �۽� �Ϸ� ��

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

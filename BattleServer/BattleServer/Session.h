#ifndef _BATTLESERVER_SERVER_SESSION_H_
#define _BATTLESERVER_SERVER_SESSION_H_

#include <Windows.h>
#include <iostream>

#include "../json/json.h"
#include "../WinHttpClient/Common/Include/WinHttpClient.h"
#include "Log.h"
#include "Packet.h"
#include "RingBuffer.h"
#include "LockFreeQueue.h"
#include "CommonProtocol.h"
//#include "BattleServer.h"

extern CConfig Config;

enum en_RES_LOGIN
{
	LOGIN_SUCCESS = 1,
	CLIENT_ERROR = 2,
	SESSIONKEY_ERROR = 3,
	VER_ERROR = 5,
	OVERLAPP_LOGIN = 6
};


typedef struct CLIENT_CONNECT_INFO
{
	//	IP, Port, Socket, ClientID 정보
	char IP[20] = { 0, };
	int Port;
	SOCKET Sock;
	unsigned __int64	ClientID;

	CLIENT_CONNECT_INFO()
	{
		Port = NULL;
		ClientID = NULL;
		Sock = INVALID_SOCKET;
	}
}CLIENTINFO;

class CBattleServer;

class CNetSession
{
public:
	enum en_SESSION_BUF
	{
		BUF = 4000,
	};

	enum en_SESSION_MODE
	{
		MODE_NONE = 1,
		MODE_AUTH = 2,
		MODE_AUTH_TO_GAME = 3,
		MODE_GAME = 4,
		MODE_LOGOUT_IN_AUTH = 5,
		MODE_LOGOUT_IN_GAME = 6,
		MODE_WAIT_LOGOUT = 7,
	};

	CNetSession();
	~CNetSession();

	virtual void OnAuth_ClientJoin() = 0;
	virtual void OnAuth_ClientLeave() = 0;
	virtual void OnAuth_Packet(CPacket *pPacket) = 0;
	virtual void OnGame_ClientJoin() = 0;
	virtual void OnGame_ClientLeave() = 0;
	virtual void OnGame_Packet(CPacket *pPacket) = 0;
	virtual void OnGame_ClientRelease() = 0;
	virtual bool OnHttp_Result_SelectAccount(string temp) = 0;
	virtual bool OnHttp_Result_SelectContents(string temp) = 0;
	virtual void OnHttp_Result_Success() = 0;
	virtual void OnRoomLeavePlayer_Auth() = 0;
	virtual void OnRoomLeavePlayer_Game() = 0;

	void	Init();
	void	Set(CBattleServer *pBattleServer);
	bool	Find_AccountNo(INT64 AccountNO);
	void	SendPacket(CPacket *pPacket);
	void	Disconnect();

	long	GetMode();
	bool	SetMode_Game();
private:


public:
	//	기본적인 IOCP 네트워크 처리를 위한 변수
	//	socket, recv, send buff, overlapped, iocount 등

	en_SESSION_MODE	_Mode;		//	세션의 상태모드
	int		_iArrayIndex;		//	Session 배열의 자기 인덱스
	int		_RoomNo;
	INT64	_AccountNo;

	CLIENT_CONNECT_INFO _ClientInfo;
	OVERLAPPED	_SendOver;
	OVERLAPPED	_RecvOver;
	CRingBuffer	_RecvQ;
	CRingBuffer _SendQ;
//	CLockFreeQueue<CPacket*>	_SendQ;
	CLockFreeQueue<CPacket*>	_CompleteRecvPacket;
	CRingBuffer _CompleteSendPacket;
//	CLockFreeQueue<CPacket*>	_CompleteSendPacket;

	int		_iSendPacketCnt;
	int		_iSendPacketSize;

	long	_SendFlag; 
	long	_IOCount;

	bool	_LogOutFlag;
	bool	_AuthToGameFlag;

	CSystemLog	*_pLog;
	CBattleServer *_pBattleServer;

	UINT64	_HeartBeat;
};

#endif _BATTLESERVER_SERVER_SESSION_H_
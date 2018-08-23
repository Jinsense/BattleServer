#ifndef _BATTLESERVER_SERVER_PLAYER_H_
#define _BATTLESERVER_SERVER_PLAYER_H_

#include "define.h"
#include "Session.h"

class CGameServer;

class CPlayer : public CNetSession
{
public:
	CPlayer();
	~CPlayer();

	//-----------------------------------------------------------
	//	�����Լ�
	//-----------------------------------------------------------
	void	OnAuth_ClientJoin();
	void	OnAuth_ClientLeave();
	void	OnAuth_Packet(CPacket *pPacket);
	void	OnGame_ClientJoin();
	void	OnGame_ClientLeave();
	void	OnGame_Packet(CPacket *pPacket);
	void	OnGame_ClientRelease();
	bool	OnHttp_Result_SelectAccount(string temp, unsigned __int64 ClientID);
	bool	OnHttp_Result_SelectContents(string temp, unsigned __int64 ClientID);
	void	OnHttp_Result_Success(unsigned __int64 ClientID);
	void	OnRoomLeavePlayer_Auth();
	void	OnRoomLeavePlayer_Game();
	void	OnHttpSendCheck();

	//-----------------------------------------------------------
	//	��Ŷó�� �Լ� - AuthPacket
	//-----------------------------------------------------------
	void	Auth_ReqLogin(CPacket *pPacket);
	void	Auth_ReqEnterRoom(CPacket *pPacket);

	//-----------------------------------------------------------
	//	��Ŷó�� �Լ� - GamePacket
	//-----------------------------------------------------------
	
	
	//-----------------------------------------------------------
	//	����� �Լ�
	//-----------------------------------------------------------
	bool	VersionCheck();
	bool	ConnectTokenCheck(char * ConnectToken, INT64 AccountNo);
	bool	OverlappLoginCheck(INT64 AccountNo);
	void	HttpJsonCall();
	bool	AccountnoCheck(INT64 AccountNo);
	BATTLEROOM * FindWaitRoom(int RoomNo);
	bool	WaitRoomCheck(BATTLEROOM * Room);
	bool	EnterTokenCheck(char * EnterToken, char * RoomToken, int RoomNo);
	bool	WaitRoomUserNumCheck(BATTLEROOM * Room);
	bool	RoomEnterSuccess(BATTLEROOM * Room);
	void	RoomEnterPlayer(BATTLEROOM * Room);
	void	RoomPlayerInfoSendPacket(BATTLEROOM * Room, INT64 AccountNo);
	void	RoomPlayerReadyCheck(BATTLEROOM * Room);

	//-----------------------------------------------------------
	//	���� �Լ�
	//-----------------------------------------------------------
	void	SetGame(CGameServer * pGameServer);
	void	UTF8toUTF16(const char *szText, WCHAR *szBuf, int iBufLen);
	void	UTF16toUTF8(WCHAR *szText, char *szBuf, int iBufLen);
	
public:
	char	_SessionKey[64];
	char	_ConnectToken[32];
	int		_Version;
	//	��Ÿ ���� ��������
	WCHAR	_Nickname[20];	//	�������� NULL
	int		_Playtime;
	int		_Playcount;
	int		_Kill;
	int		_Die;
	int		_Win;
	
	long	_LoginReq;
	long	_LoginSuccess;

	CGameServer * _pGameServer;
};

#endif _BATTLESERVER_SERVER_PLAYER_H_
#ifndef _BATTLESERVER_SERVER_PLAYER_H_
#define _BATTLESERVER_SERVER_PLAYER_H_

#include "Session.h"
#include "LanClient.h"

class CLanClient;

class CPlayer : public CNetSession
{
public:
	CPlayer();
	~CPlayer();

	void	OnAuth_ClientJoin();
	void	OnAuth_ClientLeave();
	void	OnAuth_Packet(CPacket *pPacket);
	void	OnGame_ClientJoin();
	void	OnGame_ClientLeave();
	void	OnGame_Packet(CPacket *pPacket);
	void	OnGame_ClientRelease();

	void	SetMaster(CLanClient * pMasterServer);
	void	UTF8toUTF16(const char *szText, WCHAR *szBuf, int iBufLen);
	void	UTF16toUTF8(WCHAR *szText, char *szBuf, int iBufLen);
	
public:
	char	_SessionKey[64];
	int		_Version;
	//	기타 게임 컨텐츠들
	WCHAR	_Nickname[20];	//	마지막에 NULL
	int		_Playtime;
	int		_Playcount;
	int		_Kill;
	int		_Die;
	int		_Win;

	CLanClient * _pMasterServer;
};

#endif _BATTLESERVER_SERVER_PLAYER_H_
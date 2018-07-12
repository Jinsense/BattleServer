#ifndef _BATTLESERVER_SERVER_PLAYER_H_
#define _BATTLESERVER_SERVER_PLAYER_H_

#include "Session.h"

class CPlayer : public CNetSession
{
public:
	CPlayer();
	~CPlayer();

	void OnAuth_ClientJoin();
	void OnAuth_ClientLeave();
	void OnAuth_Packet(CPacket *pPacket);
	void OnGame_ClientJoin();
	void OnGame_ClientLeave();
	void OnGame_Packet(CPacket *pPacket);
	void OnGame_ClientRelease();

	
public:
	INT64	_AccountNo;
	char	_SessionKey[64];
	int		_Version;
	//	±‚≈∏ ∞‘¿” ƒ¡≈Ÿ√˜µÈ
	int		_Playtime;
	int		_Playcount;
	int		_Kill;
	int		_Die;
	int		_Win;
};

#endif
#ifndef _BATTLESERVER_SERVER_CONFIG_H_
#define _BATTLESERVER_SERVER_CONFIG_H_

#include "Parse.h"

class CConfig
{
	enum eNumConfig
	{
		eNUM_BUF = 20,
	};
public:
	CConfig();
	~CConfig();

	bool Set();

public:
	//	NETWORK
	UINT VER_CODE;

	WCHAR BATTLE_BIND_IP[20];
	int BATTLE_BIND_IP_SIZE;
	int BATTLE_BIND_PORT;

	WCHAR MASTER_BIND_IP[20];
	int MASTER_BIND_IP_SIZE;
	int MASTER_BIND_PORT;

	WCHAR CHAT_BIND_IP[20];
	int CHAT_BIND_IP_SIZE;
	int CHAT_BIND_PORT;


	WCHAR MONITOR_BIND_IP[20];
	int MONITOR_BIND_IP_SIZE;
	int MONITOR_BIND_PORT;

	WCHAR APISERVER_SELECT_ACCOUNT[60];
	int APISERVER_SELECT_ACCOUNT_SIZE;

	WCHAR APISERVER_SELECT_CONTENTS[60];
	int APISERVER_SELECT_CONTENTS_SIZE;

	WCHAR APISERVER_UPDATE_ACCOUNT[60];
	int APISERVER_UPDATE_ACCOUNT_SIZE;

	WCHAR APISERVER_UPDATE_CONTENTS[60];
	int APISERVER_UPDATE_CONTENTS_SIZE;

	//	SYSTEM
	int WORKER_THREAD;
	int BATTLEROOM_DEFAULT_NUM;
	int BATTLEROOM_MAX_USER;
	int SEND;
	int AUTH;
	int GAME;
	int CONNECTTOKEN_RECREATE;
	int SERVER_TIMEOUT;
	int CLIENT_MAX;
	int AUTH_MAX;
	int GAME_MAX;
	char MASTERTOKEN[32];
	int MASTERTOKEN_SIZE;
	int PACKET_CODE;
	int PACKET_KEY1;
	int PACKET_KEY2;
	int LOG_LEVEL;

	CINIParse _Parse;

private:
	char IP[60];
};





#endif // !_MASTERSERVER_SERVER_CONFIG_H_


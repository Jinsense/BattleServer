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

	WCHAR MATCH_BIND_IP[20];
	int MATCH_BIND_IP_SIZE;
	int MATCH_BIND_PORT;

	WCHAR BATTLE_BIND_IP[20];
	int BATTLE_BIND_IP_SIZE;
	int BATTLE_BIND_PORT;

	WCHAR MONITOR_IP[20];
	int MONITOR_IP_SIZE;
	int MONITOR_PORT;

	//	SYSTEM
	int WORKER_THREAD;
	int SEND;
	int AUTH;
	int GAME;
	int SERVER_TIMEOUT;
	int CLIENT_MAX;
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


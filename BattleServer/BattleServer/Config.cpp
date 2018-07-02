#include <windows.h>

#include "Config.h"

CConfig::CConfig()
{
	VER_CODE = NULL;
	ZeroMemory(&MASTERTOKEN, sizeof(MASTERTOKEN));
	MASTERTOKEN_SIZE = eNUM_BUF * 2;

	ZeroMemory(&MATCH_BIND_IP, sizeof(MATCH_BIND_IP));
	MATCH_BIND_IP_SIZE = eNUM_BUF;
	MATCH_BIND_PORT = NULL;

	ZeroMemory(&BATTLE_BIND_IP, sizeof(BATTLE_BIND_IP));
	BATTLE_BIND_IP_SIZE = eNUM_BUF;
	BATTLE_BIND_PORT = NULL;

	WORKER_THREAD = NULL;
	SERVER_TIMEOUT = NULL;
	CLIENT_MAX = NULL;
	PACKET_CODE = NULL;
	PACKET_KEY1 = NULL;
	PACKET_KEY2 = NULL;
	LOG_LEVEL = NULL;

	ZeroMemory(&IP, sizeof(IP));
}

CConfig::~CConfig()
{

}

bool CConfig::Set()
{
	bool res = true;
	res = _Parse.LoadFile(L"MasterServer_Config.ini");
	if (false == res)
		return false;
	res = _Parse.ProvideArea("NETWORK");
	if (false == res)
		return false;
	res = _Parse.GetValue("VER_CODE", &VER_CODE);
	if (false == res)
		return false;
	_Parse.GetValue("MASTERTOKEN", &MASTERTOKEN[0], &MASTERTOKEN_SIZE);

	res = _Parse.GetValue("MATCH_BIND_IP", &IP[0], &MATCH_BIND_IP_SIZE);
	if (false == res)
		return false;
	_Parse.UTF8toUTF16(IP, MATCH_BIND_IP, sizeof(MATCH_BIND_IP));
	res = _Parse.GetValue("MATCH_BIND_PORT", &MATCH_BIND_PORT);
	if (false == res)
		return false;
	_Parse.GetValue("BATTLE_BIND_PORT", &IP[0], &BATTLE_BIND_IP_SIZE);
	_Parse.UTF8toUTF16(IP, BATTLE_BIND_IP, sizeof(BATTLE_BIND_IP));
	_Parse.GetValue("BATTLE_BIND_PORT", &BATTLE_BIND_PORT);
	if (false == res)
		return false;

	_Parse.ProvideArea("SYSTEM");
	res = _Parse.GetValue("WORKER_THREAD", &WORKER_THREAD);
	if (false == res)
		return false;
	_Parse.GetValue("USER_TIMEOUT", &SERVER_TIMEOUT);
	_Parse.GetValue("CLIENT_MAX", &CLIENT_MAX);
	_Parse.GetValue("PACKET_CODE", &PACKET_CODE);
	_Parse.GetValue("PACKET_KEY1", &PACKET_KEY1);
	_Parse.GetValue("PACKET_KEY2", &PACKET_KEY2);
	_Parse.GetValue("LOG_LEVEL", &LOG_LEVEL);

	return true;
}
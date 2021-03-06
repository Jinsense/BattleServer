#include <WinSock2.h>

#include "Session.h"
#include "BattleServer.h"

CNetSession::CNetSession() : _RecvQ(BUF), _SendQ(BUF), _CompleteSendPacket(BUF)
{
	_Mode = MODE_NONE;
	_iArrayIndex = NULL;
	_RoomNo = NULL;
	_RecvQ.Clear();
	_SendQ.Clear();
	_iSendPacketCnt = NULL;
	_iSendPacketSize = NULL;
	_SendFlag = false;
	_IOCount = NULL;
	_LogOutFlag = false;
	_AuthToGameFlag = false;
	_AccountNo = NULL;
	_HeartBeat = NULL;

	_pLog = _pLog->GetInstance();
}

CNetSession::~CNetSession()
{
	while (0 != _CompleteSendPacket.GetUseSize())
		//			while (0 != _pSessionArray[i]->_CompleteSendPacket.GetUseCount())
	{
		//	애초에 큐에 패킷이 남아있으면 안됨..
		CPacket *pPacket;
		_CompleteSendPacket.Dequeue((char*)&pPacket, sizeof(CPacket*));
		//				_pSessionArray[i]->_CompleteSendPacket.Dequeue(pPacket);
		pPacket->Free();
	}
}

void CNetSession::Init()
{
	_RecvQ.Initialize(RINGBUFFERSIZE);
	_SendQ.Initialize(RINGBUFFERSIZE);
	_CompleteSendPacket.Initialize(RINGBUFFERSIZE);
	return;
}

void CNetSession::Set(CBattleServer *pBattleServer)
{
	_pBattleServer = pBattleServer;
	return;
}

bool CNetSession::Find_AccountNo(INT64 AccountNo)
{
	bool Find = false;
	std::map<INT64, CNetSession*>::iterator iter;
	AcquireSRWLockExclusive(&_pBattleServer->_AccountNoMap_srwlock);
	iter = _pBattleServer->_AccountNoMap.find(AccountNo);
	if (_pBattleServer->_AccountNoMap.end() == iter)
	{
		//	Not Found
		_pBattleServer->_AccountNoMap.insert(std::make_pair(AccountNo, _pBattleServer->_pSessionArray[_iArrayIndex]));
		Find = false;
	}
	else
	{
		//	Found - 중복 로그인
		//	Map에서 기존 유저 LogoutFlag 변경
		(*iter).second->Disconnect();
		Find = true;
	}
	ReleaseSRWLockExclusive(&_pBattleServer->_AccountNoMap_srwlock);

	if(true == Find)
		_pLog->Log(L"OverlapLogin", LOG_SYSTEM, L"Overlap Login - AccountNo %d", AccountNo);
	return Find;
}

void CNetSession::SendPacket(CPacket *pPacket)
{
	pPacket->AddRef();
	pPacket->EnCode();
	_SendQ.Enqueue((char*)&pPacket, sizeof(CPacket*));
//	_SendQ.Enqueue(pPacket);
	return;
}

void CNetSession::Disconnect()
{
	shutdown(_ClientInfo.Sock, SD_BOTH);
	return;
}

long CNetSession::GetMode()
{
	return _Mode;
}

bool CNetSession::SetMode_Game()
{
	if (MODE_AUTH != _Mode)
		return false;

	_Mode = MODE_AUTH_TO_GAME;
	return true;
}

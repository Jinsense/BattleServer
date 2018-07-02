#include <WinSock2.h>

#include "Session.h"

CNetSession::CNetSession() : _RecvQ(BUF), _SendQ(BUF), _CompleteSendPacket(BUF)
{
	_Mode = MODE_NONE;
	_iArrayIndex = NULL;
	_RecvQ.Clear();
	_SendQ.Clear();
	_iSendPacketCnt = NULL;
	_iSendPacketSize = NULL;
	_SendFlag = false;
	_IOCount = NULL;
	_LogOutFlag = false;
	_AuthToGameFlag = false;

	_pLog->GetInstance();
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

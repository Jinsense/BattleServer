#include "Player.h"

CPlayer::CPlayer()
{
	_AccountNo = NULL;
	ZeroMemory(&_SessionKey, sizeof(_SessionKey));
	_Version = NULL;
}

CPlayer::~CPlayer()
{

}

void CPlayer::OnAuth_ClientJoin()
{
	//	해당 플레이어 접속으로 인한 게임상의 데이터 할당 및 준비

	return;
}

void CPlayer::OnAuth_ClientLeave()
{
	//	인증과정에서만 특별하게 사용했던 데이터가 있다면 정리 (아무것도 안할 수도 있음)

	return;
}

void CPlayer::OnAuth_Packet(CPacket *pPacket)
{
	//	로그인 패킷 확인 및 DB 데이터 로딩
	//== Auth 스레드
	//
	//en_PACKET_CS_GAME_REQ_LOGIN
	//en_PACKET_CS_GAME_RES_LOGIN
	if (0 == pPacket->GetDataSize())
	{
		_pLog->Log(L"Error", LOG_SYSTEM, L"OnAuthPacket - Packet DataSize is 0  Index : %d", this->_iArrayIndex);
		Disconnect();
		return;
	}
	WORD Type;
	pPacket->PopData((char*)&Type, sizeof(WORD));

	switch (Type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
	{
		*pPacket >> _AccountNo;
		pPacket->PopData((char*)_SessionKey, sizeof(_SessionKey));
		*pPacket >> _Version;

		_AuthToGameFlag = true;

		CPacket *pNewPacket = CPacket::Alloc();
		Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Status = 1;

		*pNewPacket << Type << Status << _AccountNo;
		SendPacket(pNewPacket);
		pNewPacket->Free();
	}
	break;
	default:
		wprintf(L"Wrong Packet Type !!\n");
		g_CrashDump->Crash();
		break;
	}
	return;
}

void CPlayer::OnGame_ClientJoin()
{
	//	게임컨텐츠 진입을 위한 데이터 준비 및 셋팅 ( 월드 맵에 캐릭터 생성, 퀘스트 준비 등등 )

	return;
}

void CPlayer::OnGame_ClientLeave()
{
	//	게임 컨텐츠상의 플레이어 정리 ( 월드맵에서 제거, 파티정리 등등 )

	return;
}

void CPlayer::OnGame_Packet(CPacket *pPacket)
{
	// 실제 게임 패킷 처리
	//== Game 스레드
	//
	//en_PACKET_CS_GAME_REQ_ECHO
	//en_PACKET_CS_GAME_RES_ECHO
	if (0 == pPacket->GetDataSize())
	{
		_pLog->Log(L"Error", LOG_SYSTEM, L"OnGamePacket - Packet DataSize is 0  Index : %d", this->_iArrayIndex);
		Disconnect();
		return;
	}
	WORD Type;
	pPacket->PopData((char*)&Type, sizeof(WORD));

	switch (Type)
	{
	case en_PACKET_CS_GAME_REQ_ECHO:
	{
		LONGLONG SendTick;
		*pPacket >> _AccountNo >> SendTick;

		CPacket *pNewPacket = CPacket::Alloc();
		Type = en_PACKET_CS_GAME_RES_ECHO;

		*pNewPacket << Type << _AccountNo << SendTick;
		SendPacket(pNewPacket);
		pNewPacket->Free();
	}
	break;
	default:
		wprintf(L"Wrong Packet Type !!\n");
		g_CrashDump->Crash();
		break;
	}
	return;
}

void CPlayer::OnGame_ClientRelease()
{
	//	해당 클라이언트의 진정한 접속 종료, 정리, 해제 등등 



	return;
}
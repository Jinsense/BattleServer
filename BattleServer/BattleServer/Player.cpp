#include "Player.h"
#include "GameServer.h"

CPlayer::CPlayer()
{
	_AccountNo = NULL;
	ZeroMemory(&_SessionKey, sizeof(_SessionKey));
	_Version = NULL;
	ZeroMemory(&_Nickname, sizeof(_Nickname));
	_Playtime = NULL;
	_Playcount = NULL;
	_Kill = NULL;
	_Die = NULL;
	_Win = NULL;
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
	//-------------------------------------------------------------
	//	로그인 패킷 확인 및 DB 데이터 로딩
	//-------------------------------------------------------------
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
	//-------------------------------------------------------------
	//	패킷 처리 - 배틀서버로 로그인 요청
	//	Type	: en_PACKET_CS_GAME_REQ_LOGIN
	//	INT64	: AccountNo
	//	char	: SessionKey[64]
	//	char	: ConnectToken[32]
	//	UINT	: Ver_Code
	//	
	//	응답	: en_PACKET_CS_GAME_RES_LOGIN
	//	WORD	: Type
	//	INT64	: AccountNo
	//	BYTE	: Result
	//-------------------------------------------------------------
	case en_PACKET_CS_GAME_REQ_LOGIN:
	{
		*pPacket >> _AccountNo;
		pPacket->PopData((char*)_SessionKey, sizeof(_SessionKey));
		*pPacket >> _Version;

		//	Config의 버전 코드와 맞는지 비교를 한다.
		if (_Version != Config.VER_CODE)
		{
			//	버전 코드가 다를 경우 로그를 남기고 버전오류 응답을 보낸 후 끊는다.
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM,
				const_cast<WCHAR*>(L"Ver_Code Not Same [AccountNo : %d]"), _AccountNo);
			CPacket * newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Result = VER_ERROR;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;
		}
		//	중복 로그인 검사
		if (true == Find_AccountNo(_AccountNo))
		{
			//	중복로그인 패킷 전송
			//	기존 로그인 유저는 함수 내부에서 LogoutFlag 변경 처리
			CPacket *newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Result = OVERLAPP_LOGIN;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;
		}		
		//-------------------------------------------------------------
		//	JSON 데이터 - select 요청
		//-------------------------------------------------------------
		//	RingBuffer-메모리풀 생성하여 HttpQueue에 저장한 후 이벤트 호출
		WORD Type = SELECT;
		CRingBuffer *pBuffer = _pGameServer->_HttpPool->Alloc();
		pBuffer->Clear();
		pBuffer->Enqueue((char*)&Type, sizeof(Type));
		pBuffer->Enqueue((char*)&_iArrayIndex, sizeof(_iArrayIndex));
		pBuffer->Enqueue((char*)&_AccountNo, sizeof(_AccountNo));
		_pGameServer->_HttpQueue.Enqueue(pBuffer);
		SetEvent(_pGameServer->_hHttpEvent);
	}
	break;
	//------------------------------------------------------------
	//	배틀서버 방 입장 요청
	//	Type	: en_PACKET_CS_GAME_REQ_ENTER_ROOM
	//	INT64	: AccountNo
	//	int		: RoomNo
	//	char	: EnterToken[32]
	//
	//	응답	: en_PACKET_CS_GAME_RES_ENTER_ROOM
	//	INT64	: AccountNo
	//	int		: RoomNo
	//	BYTE	: MaxUser
	//	BYTE	: Result
	//------------------------------------------------------------
	case en_PACKET_CS_GAME_REQ_ENTER_ROOM:
	{
		INT64 AccountNo = NULL;
		int RoomNo = NULL;
		char EnterToken[32] = { 0, };
		*pPacket >> AccountNo >> RoomNo;
		pPacket->PopData((char*)&EnterToken, sizeof(EnterToken));
		//	특정 방 입장 요청
		//	AccountNo는 버그 감지 및 테스트용
		if (AccountNo != _AccountNo)
		{
			//	계정번호가 다름 - 로그 남기고 끊음
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"AccountNo Not Same [My AccountNo : %d, RecvAccountNo]"), _AccountNo, AccountNo);
			Disconnect();
		}
		//	BattleRoomMap에서 방 번호로 방을 찾음
		bool NotFind = false;
		bool NotReadyRoom = false;
		std::map<int, BATTLEROOM*>::iterator iter;
		AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
		iter = _pGameServer->_WaitRoomMap.find(RoomNo);
		if (iter == _pGameServer->_WaitRoomMap.end())
			NotFind = true;
		if (true == (*iter).second->RoomReady)
			NotReadyRoom = true;
		ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
		//	방이 존재하는지 확인 후 없는 방일 경우 방 없음 패킷 전송
		if (true == NotFind)
		{
			CPacket * newPacket = CPacket::Alloc();
			WORD MaxUser = Config.BATTLEROOM_MAX_USER;
			WORD Result = NOT_FINDROOM;
			Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
			*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;
		}
		//	대기방인지 확인
		if (true == NotReadyRoom)
		{
			//	대기방이 아님 패킷 전송
			CPacket * newPacket = CPacket::Alloc();
			WORD MaxUser = Config.BATTLEROOM_MAX_USER;
			WORD Result = NOT_READYROOM;
			Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
			*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;
		}
		//	EnterToken 일치할 경우 입장 허용
		if (0 != strncmp(EnterToken, (*iter).second->Entertoken, sizeof(EnterToken)))
		{
			//	다른경우 로그 남기고 EnterToken 다름 패킷 전송
			CPacket * newPacket = CPacket::Alloc();
			WORD MaxUser = Config.BATTLEROOM_MAX_USER;
			WORD Result = ENTERTOKEN_FAIL;
			Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
			*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;

		}
		//	방에 유저가 들어갈 수 있는지 인원수 체크
		if (0 >= ((*iter).second->MaxUser - (*iter).second->CurUser))
		{
			//	들어갈 자리가 없음 패킷 전송
			CPacket * newPacket = CPacket::Alloc();
			WORD MaxUser = Config.BATTLEROOM_MAX_USER;
			WORD Result = ROOMUSER_MAX;
			Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
			*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;	
		}
		(*iter).second->CurUser++;
		//	방에 입장 인원이 꽉찼을 경우 마스터 서버로 대기 방 닫힘 패킷 전송
		if ((*iter).second->CurUser == (*iter).second->MaxUser)
		{
			CPacket * CloseRoomPacket = CPacket::Alloc();
			Type = en_PACKET_BAT_MAS_REQ_CLOSED_ROOM;
			*CloseRoomPacket >> Type >> (*iter).second->RoomNo;
			_pGameServer->_pMaster->SendPacket(CloseRoomPacket);
			CloseRoomPacket->Free();
			(*iter).second->RoomReady = true;
		}
		//	_RoomNo에 방 번호와 RoomPlayer 구조체에 Session Index번호 지정
		//	현재는 락을 걸지 않았지만 차후 문제가 생길경우 락을 추가하여 해결하자
		_RoomNo = RoomNo;
		RoomPlayerInfo Info;
		Info.AccountNo = _AccountNo;
		Info.Index = _iArrayIndex;
		(*iter).second->RoomPlayer.push_back(Info);
		//	방 입장 응답 패킷 전송
		CPacket * newPacket = CPacket::Alloc();
		WORD MaxUser = Config.BATTLEROOM_MAX_USER;
		WORD Result = enENTERROOM_RESULT::SUCCESS;
		Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
		SendPacket(newPacket);
		newPacket->Free();
		//	정상적으로 입장이 성공할 경우 방에 유저가 추가됨 패킷도 추가 전송
		//	새로 접속한 클라이언트에게도 패킷을 보내줌
		//	GameServer.cpp에서 패킷을 보내려면 Index번호가 필요함
		CPacket * AddPacket = CPacket::Alloc();
		*AddPacket >> _RoomNo >> _AccountNo;
		AddPacket->PushData((char*)&_Nickname, sizeof(_Nickname));
		*AddPacket >> _Playcount >> _Playtime >> _Kill >> _Die >> _Win;
		AddPacket->AddRef();
		for (auto j = (*iter).second->RoomPlayer.begin(); j != (*iter).second->RoomPlayer.end(); j++)
		{
			_pBattleServer->_pSessionArray[(*j).Index]->SendPacket(AddPacket);
			AddPacket->Free();
		}
		AddPacket->Free();
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
	//	패킷 처리
		
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

bool CPlayer::OnHttp_Result_SelectAccount(string temp)
{
	Json::Reader reader;
	Json::Value result;
	bool Res = reader.parse(temp, result);
	if (!Res)
	{
		_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Failed to parse Json [AccountNo : %d]"), _AccountNo);
		CPacket * newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Status = CLIENT_ERROR;
		*newPacket << Type << Status;
		SendPacket(newPacket);
//		SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
		newPacket->Free();
		return false;
	}
	int ResNum = result["result"].asInt();
	if (LOGIN_SUCCESS != ResNum)
	{
		BYTE Status = CLIENT_ERROR;
		CPacket * newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		*newPacket << Type << Status;
		SendPacket(newPacket);
//		SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
		newPacket->Free();
		return false;
	}

	string sessionkey = result["sessionkey"].asCString();
	if (0 != strncmp(sessionkey.c_str(), _SessionKey, sizeof(_SessionKey)))
	{
		//	세션키가 다름 응답 후 끊기
		CPacket * newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Status = SESSIONKEY_ERROR;
		*newPacket << Type << Status;
		SendPacket(newPacket);
//		SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
		newPacket->Free();
		return false;
	}
	//	Nickname 저장 - char로 변환 후 wchar로 저장
	string Nickname = result["nickname"].asCString();
	char Temp[20] = { 0 , };
	strcpy_s(Temp, sizeof(Temp), Nickname.c_str());
	UTF8toUTF16(Temp, _Nickname, sizeof(Temp));

	return true;
}

bool CPlayer::OnHttp_Result_SelectContents(string temp)
{
	Json::Reader reader;
	Json::Value result;
	bool Res = reader.parse(temp, result);
	if (!Res)
	{
		_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Failed to parse Json [AccountNo : %d]"), _AccountNo);
		CPacket * newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Status = CLIENT_ERROR;
		*newPacket << Type << Status;
		SendPacket(newPacket);
		//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
		newPacket->Free();
		return false;
	}
	int ResNum = result["result"].asInt();
	if (LOGIN_SUCCESS != ResNum)
	{
		BYTE Status = CLIENT_ERROR;
		CPacket * newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		*newPacket << Type << Status;
		SendPacket(newPacket);
		//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
		newPacket->Free();
		return false;
	}
	_Playtime = result["play"].asInt();
	_Playcount = result["playercount"].asInt();
	_Kill = result["kill"].asInt();
	_Die = result["Die"].asInt();
	_Win = result["Win"].asInt();
	return true;
}

void CPlayer::OnHttp_Result_Success()
{
	CPacket *pNewPacket = CPacket::Alloc();
	WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
	BYTE Status = 1;

	*pNewPacket << Type << Status << _AccountNo;
	SendPacket(pNewPacket);
	pNewPacket->Free();
	return;
}

void CPlayer::SetGame(CGameServer * pGameServer)
{
	_pGameServer = pGameServer;
	return;
}

void CPlayer::UTF8toUTF16(const char *szText, WCHAR *szBuf, int iBufLen)
{
	int iRe = MultiByteToWideChar(CP_UTF8, 0, szText, strlen(szText), szBuf, iBufLen);
	if (iRe < iBufLen)
		szBuf[iRe] = L'\0';
	return;
}

void CPlayer::UTF16toUTF8(WCHAR *szText, char *szBuf, int iBufLen)
{
	int iRe = WideCharToMultiByte(CP_UTF8, 0, szText, lstrlenW(szText), szBuf, iBufLen, NULL, NULL);
	return;
}
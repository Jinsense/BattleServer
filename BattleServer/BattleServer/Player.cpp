#include "Player.h"
#include "GameServer.h"

CPlayer::CPlayer()
{
	_AccountNo = NULL;
	ZeroMemory(&_SessionKey, sizeof(_SessionKey));
	ZeroMemory(&_ConnectToken, sizeof(_ConnectToken));
	_Version = NULL;
	ZeroMemory(&_Nickname, sizeof(_Nickname));
	_Playtime = NULL;
	_Playcount = NULL;
	_Kill = NULL;
	_Die = NULL;
	_Win = NULL;
	_pGameServer = nullptr;

	_LoginReq = false;
	_LoginSuccess = false;
}

CPlayer::~CPlayer()
{

}

void CPlayer::OnAuth_ClientJoin()
{
	//	해당 플레이어 접속으로 인한 게임상의 데이터 할당 및 준비
	_RoomNo = NULL;
	_AccountNo = NULL;
	ZeroMemory(&_SessionKey, sizeof(_SessionKey));
	ZeroMemory(&_ConnectToken, sizeof(_ConnectToken));
	_Version = NULL;
	ZeroMemory(&_Nickname, sizeof(_Nickname));
	_Playtime = NULL;
	_Playcount = NULL;
	_Kill = NULL;
	_Die = NULL;
	_Win = NULL;
	_ClientInfo.ClientID = NULL;
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
		Auth_ReqLogin(pPacket);
		_HeartBeat = GetTickCount64();
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
		Auth_ReqEnterRoom(pPacket);
		_HeartBeat = GetTickCount64();
	}
	break;	
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
	{
		_HeartBeat = GetTickCount64();
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
	//------------------------------------------------------------
	//	게임 컨텐츠상의 플레이어 정리 ( 월드맵에서 제거, 파티정리 등등 )
	//------------------------------------------------------------
	//	플레이 중인 방에서 유저가 나감을 다른 유저들에게 전달
	OnRoomLeavePlayer_Game();
	return;
}

void CPlayer::OnGame_Packet(CPacket *pPacket)
{
	// 실제 게임 패킷 처리
	//== Game 스레드	
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
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
	{
		_HeartBeat = GetTickCount64();
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
	_ClientInfo.ClientID = NULL;
	_RoomNo = NULL;
	_AccountNo = NULL;
	ZeroMemory(&_SessionKey, sizeof(_SessionKey));
	ZeroMemory(&_ConnectToken, sizeof(_ConnectToken));
	_Version = NULL;
	InterlockedCompareExchange(&_LoginSuccess, false, true);
	return;
}

bool CPlayer::OnHttp_Result_SelectAccount(string temp, unsigned __int64 ClientID)
{
	Json::Reader reader;
	Json::Value result;

	bool Res = reader.parse(temp, result);
	if (!Res)
	{
		if (ClientID == _ClientInfo.ClientID && NULL != _AccountNo)
		{
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Failed to parse Json [AccountNo : %d]"), _AccountNo);
			CPacket * newPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Result = CLIENT_ERROR;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			//		SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
		}
		return false;
	}

	int ResNum = result["result"].asInt();
	if (LOGIN_SUCCESS != ResNum)
	{
		if (ClientID == _ClientInfo.ClientID && NULL != _AccountNo)
		{
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Json Result Error [AccountNo : %d]"), _AccountNo);

			BYTE Result = CLIENT_ERROR;
			CPacket * newPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			//		SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
		}
		return false;
	}

	string sessionkey = result["sessionkey"].asCString();
	if (0 != strncmp(sessionkey.c_str(), _SessionKey, sizeof(_SessionKey)))
	{
		if (ClientID == _ClientInfo.ClientID && NULL != _AccountNo)
		{
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"SessionKey Error [AccountNo : %d]"), _AccountNo);
			//	세션키가 다름 응답 후 끊기
			CPacket * newPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Result = SESSIONKEY_ERROR;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			//		SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
		}
		return false;
	}
	//	Nickname 저장 - char로 변환 후 wchar로 저장
	string Nickname = result["nick"].asCString();
	char Temp[20] = { 0 , };
	strcpy_s(Temp, sizeof(Temp), Nickname.c_str());
	UTF8toUTF16(Temp, _Nickname, sizeof(Temp));
	return true;
}

bool CPlayer::OnHttp_Result_SelectContents(string temp, unsigned __int64 ClientID)
{
	Json::Reader reader;
	Json::Value result;
	bool Res = reader.parse(temp, result);
	if (!Res)
	{
		if (ClientID == _ClientInfo.ClientID && NULL != _AccountNo)
		{
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Failed to parse Json [AccountNo : %d]"), _AccountNo);

			CPacket * newPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Result = CLIENT_ERROR;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
		}
		return false;
	}
	int ResNum = result["result"].asInt();
	if (LOGIN_SUCCESS != ResNum)
	{
		if (ClientID == _ClientInfo.ClientID && NULL != _AccountNo)
		{
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Json Result Error [AccountNo : %d]"), _AccountNo);
			BYTE Result = CLIENT_ERROR;
			CPacket * newPacket = CPacket::Alloc();
			WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
		}
		return false;
	}

	_Playtime = result["play"].asInt();
	_Playcount = result["playercount"].asInt();
	_Kill = result["kill"].asInt();
	_Die = result["Die"].asInt();
	_Win = result["Win"].asInt();
	return true;
}

void CPlayer::OnHttp_Result_Success(unsigned __int64 ClientID)
{
	if (ClientID == _ClientInfo.ClientID && ClientID != NULL && _AccountNo != NULL && false == InterlockedCompareExchange(&_LoginReq, true, false) && true == _pGameServer->OnHttpReqRemove(_AccountNo))
	{
		InterlockedCompareExchange(&_LoginSuccess, true, false);
	}
	return;
}

void CPlayer::Auth_ReqLogin(CPacket *pPacket)
{
	*pPacket >> _AccountNo;
	pPacket->PopData((char*)_SessionKey, sizeof(_SessionKey));
	pPacket->PopData((char*)_ConnectToken, sizeof(_ConnectToken));
	*pPacket >> _Version;

	if (false == VersionCheck())
		return;
	
	if (false == ConnectTokenCheck(_ConnectToken, _AccountNo))
		return;

	if (false == AccountnoCheck(_AccountNo))
		return;

	if (false == OverlappLoginCheck(_AccountNo))
		return;
	HttpJsonCall();
	return;
}

void CPlayer::Auth_ReqEnterRoom(CPacket *pPacket)
{
	INT64 AccountNo = NULL;
	int RoomNo = NULL;
	char EnterToken[32] = { 0, };
	*pPacket >> AccountNo >> RoomNo;
	pPacket->PopData((char*)&EnterToken, sizeof(EnterToken));

	if (false == AccountnoCheck(AccountNo))
		return;
	BATTLEROOM * Room = FindWaitRoom(RoomNo);
	if (nullptr == Room)
		return;
	if (false == WaitRoomCheck(Room))
		return;
	if (false == EnterTokenCheck(EnterToken, Room->Entertoken, Room->RoomNo))
		return;
	if (false == WaitRoomUserNumCheck(Room))
		return;

	if(true == RoomEnterSuccess(Room))			//	방 입장 성공 패킷 전송
		RoomPlayerReadyCheck(Room);			//	대기방 플레이 전환 검사
	RoomEnterPlayer(Room);			//	기존 유저 + 방 입장한 유저에게 입장한 유저 정보 전송
	RoomPlayerInfoSendPacket(Room, AccountNo);	//	기존에 존재하던 유저들 정보를 새로 입장한 유저에게 전송
	
	return;
}

bool CPlayer::VersionCheck()
{
	//	Config의 버전 코드와 맞는지 비교를 한다.
	if (_Version != Config.VER_CODE)
	{
		//	버전 코드가 다를 경우 로그를 남기고 버전오류 응답을 보낸 후 끊는다.
		_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM,
			const_cast<WCHAR*>(L"Ver_Code Not Same [AccountNo : %d]"), _AccountNo);

		CPacket * newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Result = VER_ERROR;
		*newPacket << Type << _AccountNo << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

bool CPlayer::ConnectTokenCheck(char * ConnectToken, INT64 AccountNo)
{
	if (0 != strncmp(ConnectToken, _pGameServer->_CurConnectToken, sizeof(_pGameServer->_CurConnectToken)))
	{
		if (0 != strncmp(ConnectToken, _pGameServer->_OldConnectToken, sizeof(_pGameServer->_OldConnectToken)))
		{
			if (AccountNo == _AccountNo && NULL != _AccountNo)
			{
				_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Connect Error [AccountNo : %d]"), _AccountNo);
				//	다른경우 로그 남기고 ConnectToken 다름 패킷 전송
				CPacket * newPacket = CPacket::Alloc();
				BYTE Result = CLIENT_ERROR;
				WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
				*newPacket << Type << _AccountNo << Result;
				SendPacket(newPacket);
				newPacket->Free();
			}
			return false;
		}
	}
	return true;
}

bool CPlayer::OverlappLoginCheck(INT64 AccountNo)
{
	//	중복 로그인 검사
	if (true == Find_AccountNo(AccountNo) && _AccountNo == AccountNo && NULL != _AccountNo)
	{
		//	중복로그인 패킷 전송
		//	기존 로그인 유저는 함수 내부에서 LogoutFlag 변경 처리
		CPacket *newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Result = OVERLAPP_LOGIN;
		*newPacket << Type << AccountNo << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

void CPlayer::HttpJsonCall()
{
	//-------------------------------------------------------------
	//	JSON 데이터 - select 요청
	//-------------------------------------------------------------
	WORD Type = SELECT;
	int Index = _iArrayIndex;
	INT64 AccountNo = _AccountNo;
	unsigned __int64 ClientID = _ClientInfo.ClientID;
	//	HttpReqMap에서 로그인 요청 상태인지 확인
	bool Find = false;
	AcquireSRWLockExclusive(&_pGameServer->_HttpReq_lock);
	if (_pGameServer->_HttpReqMap.find(AccountNo) == _pGameServer->_HttpReqMap.end()) {
		// not found
		_pGameServer->_HttpReqMap.insert(make_pair(AccountNo, AccountNo));
	}
	else {
		// found
		Find = true;
	}
	ReleaseSRWLockExclusive(&_pGameServer->_HttpReq_lock);

	if (true == Find)
		return;

	//	RingBuffer-메모리풀 생성하여 HttpQueue에 저장한 후 이벤트 호출
	CRingBuffer *pBuffer = _pGameServer->_HttpPool->Alloc();
//	pBuffer->Initialize(500);
	pBuffer->Clear();
	pBuffer->Enqueue((char*)&Type, sizeof(Type));
	pBuffer->Enqueue((char*)&Index, sizeof(Index));
	pBuffer->Enqueue((char*)&AccountNo, sizeof(AccountNo));
	//	ClientID 저장 후 Http 요청
	pBuffer->Enqueue((char*)&ClientID, sizeof(ClientID));

//	_LoginReq = false;
	InterlockedCompareExchange(&_LoginReq, false, true);
	_pGameServer->_HttpQueue.Enqueue(pBuffer);
	SetEvent(_pGameServer->_hHttpEvent);
	return;
}

bool CPlayer::AccountnoCheck(INT64 AccountNo)
{
	//	AccountNo는 버그 감지 및 테스트용
	if (AccountNo != _AccountNo)
	{
		//	계정번호가 다름 - 로그 남기고 끊음
		_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"AccountNo Not Same [My AccountNo : %d, RecvAccountNo]"), _AccountNo, AccountNo);
		Disconnect();
		return false;
	}
	return true;
}

BATTLEROOM * CPlayer::FindWaitRoom(int RoomNo)
{
	//	WaitRoomMap에서 방 번호로 방을 찾음
	bool NotFind = false;
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	iter = _pGameServer->_WaitRoomMap.find(RoomNo);
	if (iter == _pGameServer->_WaitRoomMap.end())
		NotFind = true;
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	//	방이 존재하는지 확인 후 없는 방일 경우 방 없음 패킷 전송
	if (true == NotFind)
	{
		CPacket * newPacket = CPacket::Alloc();
		BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
		BYTE Result = NOT_FINDROOM;
		WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket << Type << _AccountNo << RoomNo << MaxUser << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return nullptr;
	}
	return (*iter).second;
}

bool CPlayer::WaitRoomCheck(BATTLEROOM * Room)
{
	//	대기방인지 확인
	if (true == Room->RoomReady)
	{
		//	대기방이 아님 패킷 전송
		CPacket * newPacket = CPacket::Alloc();
		BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
		BYTE Result = NOT_READYROOM;
		WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket << Type << _AccountNo << Room->RoomNo << MaxUser << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

bool CPlayer::EnterTokenCheck(char * EnterToken, char * RoomToken, int RoomNo)
{
	//	EnterToken 일치할 경우 입장 허용
	if (0 != strncmp(EnterToken, RoomToken, sizeof(EnterToken)))
	{
		//	다른경우 로그 남기고 EnterToken 다름 패킷 전송
		CPacket * newPacket = CPacket::Alloc();
		BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
		BYTE Result = ENTERTOKEN_FAIL;
		WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket << Type << _AccountNo << RoomNo << MaxUser << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

bool CPlayer::WaitRoomUserNumCheck(BATTLEROOM * Room)
{
	//	방에 유저가 들어갈 수 있는지 인원수 체크
	if (0 >= (Room->MaxUser - Room->CurUser))
	{
		//	들어갈 자리가 없음 패킷 전송
		CPacket * newPacket = CPacket::Alloc();
		BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
		BYTE Result = ROOMUSER_MAX;
		WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket << Type << _AccountNo << Room->RoomNo << MaxUser << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

bool CPlayer::RoomEnterSuccess(BATTLEROOM * Room)
{
	//	_RoomNo에 방 번호와 RoomPlayer 구조체에 Session Index번호 지정
	//	현재는 락을 걸지 않았지만 차후 문제가 생길경우 락을 추가하여 해결하자
	_RoomNo = Room->RoomNo;
	int CurUser = NULL;
	RoomPlayerInfo * pInfo = _pGameServer->_RoomPlayerPool->Alloc();
	pInfo->AccountNo = _AccountNo;
	pInfo->Index = _iArrayIndex;
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	Room->RoomPlayer.push_back(pInfo);
	CurUser = InterlockedIncrement(&Room->CurUser);
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);

	//	방 입장 응답 패킷 전송
	CPacket * newPacket = CPacket::Alloc();
	BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
	BYTE Result = enENTERROOM_RESULT::SUCCESS;
	WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
	*newPacket << Type << _AccountNo << _RoomNo << MaxUser << Result;
	SendPacket(newPacket);
	newPacket->Free();

	if (Room->MaxUser == CurUser)
		return true;
	else
		return false;
}

void CPlayer::RoomEnterPlayer(BATTLEROOM * Room)
{
	//	정상적으로 입장이 성공할 경우 방에 유저가 추가됨 패킷도 추가 전송
	//	새로 접속한 클라이언트에게도 패킷을 보내줌
	//	GameServer.cpp에서 패킷을 보내려면 Index번호가 필요함
	WORD Type = en_PACKET_CS_GAME_RES_ADD_USER;
	/*CPacket * AddPacket = CPacket::Alloc();
	*AddPacket << Type << _RoomNo << _AccountNo;
	AddPacket->PushData((char*)&_Nickname, sizeof(_Nickname));
	*AddPacket << _Playcount << _Playtime << _Kill << _Die << _Win;
	AddPacket->AddRef();*/
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	for (auto j = Room->RoomPlayer.begin(); j != Room->RoomPlayer.end(); j++)
	{
		CPacket * AddPacket = CPacket::Alloc();
		*AddPacket << Type << _RoomNo << _AccountNo;
		AddPacket->PushData((char*)&_Nickname, sizeof(_Nickname));
		*AddPacket << _Playcount << _Playtime << _Kill << _Die << _Win;
		_pBattleServer->_pSessionArray[(*j)->Index]->SendPacket(AddPacket);
		AddPacket->Free();
	}
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	/*AddPacket->Free();*/
	return;
}

void CPlayer::RoomPlayerInfoSendPacket(BATTLEROOM * Room, INT64 AccountNo)
{
	//-----------------------------------------------------------
	//	새로 입장한 플레이어에게 기존 유저 정보 전송
	//-----------------------------------------------------------
	std::list<RoomPlayerInfo*>::iterator iter;
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	for (iter = Room->RoomPlayer.begin(); iter != Room->RoomPlayer.end(); iter++)
	{
		if ((*iter)->AccountNo == AccountNo)
			continue;
		CPlayer * pPlayer = &_pGameServer->_pPlayer[(*iter)->Index];
		WORD Type = en_PACKET_CS_GAME_RES_ADD_USER;
		CPacket * pPacket = CPacket::Alloc();
		*pPacket << Type << Room->RoomNo << (*iter)->AccountNo;
		pPacket->PushData((char*)&pPlayer->_Nickname, sizeof(pPlayer->_Nickname));
		*pPacket << pPlayer->_Playcount << pPlayer->_Playtime << pPlayer->_Kill << pPlayer->_Die << pPlayer->_Win;
		SendPacket(pPacket);
		pPacket->Free();
	}
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	return;
}

void CPlayer::RoomPlayerReadyCheck(BATTLEROOM * Room)
{
	//-----------------------------------------------------------
	//	방의 인원이 전부 꽉찼는지 검사 후 RoomReady 플래그 전환
	//	플래그 바꾸면 Auth에서 감지 후 방 시작 패킷 전송
	//-----------------------------------------------------------
	Room->RoomReady = true;
	//	마스터 서버로 대기 방 닫힘 패킷 전송
	AcquireSRWLockExclusive(&_pGameServer->_ClosedRoom_lock);
	_pGameServer->_ClosedRoomlist.push_back(Room->RoomNo);
	ReleaseSRWLockExclusive(&_pGameServer->_ClosedRoom_lock);

	CPacket * CloseRoomPacket = CPacket::Alloc();
	WORD Type = en_PACKET_BAT_MAS_REQ_CLOSED_ROOM;
	*CloseRoomPacket << Type << Room->RoomNo << _pGameServer->_Sequence;
	_pGameServer->_pMaster->SendPacket(CloseRoomPacket);
	CloseRoomPacket->Free();

	return;
}

void CPlayer::OnHttpSendCheck()
{
	if (true == InterlockedCompareExchange(&_LoginSuccess, false, true))
	{
		CPacket *pNewPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Result = LOGIN_SUCCESS;

		*pNewPacket << Type << _AccountNo << Result;
		SendPacket(pNewPacket);
		pNewPacket->Free();
	}
	return;
}

void CPlayer::OnRoomLeavePlayer_Auth()
{
	bool End = false;
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	iter = _pGameServer->_WaitRoomMap.find(_RoomNo);
	if (iter != _pGameServer->_WaitRoomMap.end())
	{
		if (false == (*iter).second->PlayReady)
		{
			_pLog->Log(const_cast<WCHAR*>(L"LeaveRoom"), LOG_SYSTEM, const_cast<WCHAR*>(L"[RoomNo : %d] AccountNo : %d"), _RoomNo, _AccountNo);

			//	마스터 서버로 배틀 서버의 대기방에서 유저가 나감 패킷 전송
			CPacket *pPacket = CPacket::Alloc();
			WORD Type = en_PACKET_BAT_MAS_REQ_LEFT_USER;
			*pPacket << Type << _RoomNo << _AccountNo << _pGameServer->_Sequence;
			_pGameServer->_pMaster->SendPacket(pPacket);
			pPacket->Free();
		}
		//	방의 리스트에서 해당 유저 삭제
		for (auto i = (*iter).second->RoomPlayer.begin(); i != (*iter).second->RoomPlayer.end();)
		{
			if (_AccountNo == (*i)->AccountNo)
			{
				_pGameServer->_RoomPlayerPool->Free(*i);
				i = (*iter).second->RoomPlayer.erase(i);
				InterlockedDecrement(&(*iter).second->CurUser);
				break;
			}
			else
				i++;
		}
	}
	else
		End = true;
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	if (true == End)
		return;

	if (true == (*iter).second->RoomPlayer.empty())
		return;

	//	해당 방의 유저들에게 해당 플레이어 방에서 나감 패킷 전송

	WORD Type = en_PACKET_CS_GAME_RES_REMOVE_USER;

	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	for (auto j = (*iter).second->RoomPlayer.begin(); j != (*iter).second->RoomPlayer.end(); j++)
	{
		if (_AccountNo == (*j)->AccountNo)
			continue;
		CPacket *newPacket = CPacket::Alloc();
		*newPacket << Type << _RoomNo << _AccountNo;
		_pGameServer->_pSessionArray[(*j)->Index]->SendPacket(newPacket);
		newPacket->Free();
	}
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	return;
}

void CPlayer::OnRoomLeavePlayer_Game()
{
	std::map<int, BATTLEROOM*>::iterator Map_iter;
	std::list<RoomPlayerInfo*>::iterator Room_iter;
	//	CPacket * pPacket = CPacket::Alloc();
	WORD Type = en_PACKET_CS_GAME_RES_LEAVE_USER;
	//	*pPacket << Type << _RoomNo << _AccountNo;
	//	pPacket->AddRef();
	AcquireSRWLockExclusive(&_pGameServer->_PlayRoom_lock);
	Map_iter = _pGameServer->_PlayRoomMap.find(_RoomNo);
	for (Room_iter = (*Map_iter).second->RoomPlayer.begin(); Room_iter != (*Map_iter).second->RoomPlayer.end();)
	{
		if ((*Room_iter)->AccountNo == _AccountNo)
		{
			_pGameServer->_RoomPlayerPool->Free(*Room_iter);
			Room_iter = (*Map_iter).second->RoomPlayer.erase(Room_iter);
			continue;
		}
		else
		{
			CPacket * pPacket = CPacket::Alloc();
			*pPacket << Type << _RoomNo << _AccountNo;
			SendPacket(pPacket);
			pPacket->Free();
			Room_iter++;
		}
	}
	ReleaseSRWLockExclusive(&_pGameServer->_PlayRoom_lock);
	//	pPacket->Free();
	return;
}

void CPlayer::SetGame(CGameServer * pGameServer)
{
	if (nullptr == pGameServer)
		g_CrashDump->Crash();
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
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
}

CPlayer::~CPlayer()
{

}

void CPlayer::OnAuth_ClientJoin()
{
	//	�ش� �÷��̾� �������� ���� ���ӻ��� ������ �Ҵ� �� �غ�

	return;
}

void CPlayer::OnAuth_ClientLeave()
{
	//	�������������� Ư���ϰ� ����ߴ� �����Ͱ� �ִٸ� ���� (�ƹ��͵� ���� ���� ����)

	return;
}

void CPlayer::OnAuth_Packet(CPacket *pPacket)
{
	//-------------------------------------------------------------
	//	�α��� ��Ŷ Ȯ�� �� DB ������ �ε�
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
	//	��Ŷ ó�� - ��Ʋ������ �α��� ��û
	//	Type	: en_PACKET_CS_GAME_REQ_LOGIN
	//	INT64	: AccountNo
	//	char	: SessionKey[64]
	//	char	: ConnectToken[32]
	//	UINT	: Ver_Code
	//	
	//	����	: en_PACKET_CS_GAME_RES_LOGIN
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
	//	��Ʋ���� �� ���� ��û
	//	Type	: en_PACKET_CS_GAME_REQ_ENTER_ROOM
	//	INT64	: AccountNo
	//	int		: RoomNo
	//	char	: EnterToken[32]
	//
	//	����	: en_PACKET_CS_GAME_RES_ENTER_ROOM
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
	//	���������� ������ ���� ������ �غ� �� ���� ( ���� �ʿ� ĳ���� ����, ����Ʈ �غ� ��� )

	return;
}

void CPlayer::OnGame_ClientLeave()
{
	//------------------------------------------------------------
	//	���� ���������� �÷��̾� ���� ( ����ʿ��� ����, ��Ƽ���� ��� )
	//------------------------------------------------------------
	//	�÷��� ���� �濡�� ������ ������ �ٸ� �����鿡�� ����
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

void CPlayer::OnGame_Packet(CPacket *pPacket)
{
	// ���� ���� ��Ŷ ó��
	//== Game ������	
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
	//	��Ŷ ó��
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
	//	�ش� Ŭ���̾�Ʈ�� ������ ���� ����, ����, ���� ��� 



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
		//	����Ű�� �ٸ� ���� �� ����
		CPacket * newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Status = SESSIONKEY_ERROR;
		*newPacket << Type << Status;
		SendPacket(newPacket);
//		SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
		newPacket->Free();
		return false;
	}

	//	Nickname ���� - char�� ��ȯ �� wchar�� ����
	string Nickname = result["nick"].asCString();
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
	BYTE Result = LOGIN_SUCCESS;
//	BYTE Result = CLIENT_ERROR;

	*pNewPacket << Type << _AccountNo << Result;
	SendPacket(pNewPacket);
	pNewPacket->Free();
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
	
	if (false == OverlappLoginCheck())
		return;
	HttpJsonCall();
	return;
}

void CPlayer::Auth_ReqEnterRoom(CPacket *pPacket)
{
	INT64 AccountNo = NULL;
//	int RoomNo = NULL;
	char EnterToken[32] = { 0, };
	*pPacket >> AccountNo >> _RoomNo;
	pPacket->PopData((char*)&EnterToken, sizeof(EnterToken));

	if (false == AccountnoCheck(AccountNo))
		return;
	BATTLEROOM * Room = FindWaitRoom(_RoomNo);
	if (nullptr == Room)
		return;
	if (false == WaitRoomCheck(Room->RoomReady))
		return;
	if (false == EnterTokenCheck(EnterToken, Room->Entertoken))
		return;
	if (false == WaitRoomUserNumCheck(Room))
		return;

	RoomEnterSuccess(Room);			//	�� ���� ���� ��Ŷ ����
	RoomEnterPlayer(Room);			//	���� ���� + �� ������ �������� ������ ���� ���� ����
	RoomPlayerInfoSendPacket(Room, AccountNo);	//	������ �����ϴ� ������ ������ ���� ������ �������� ����
	RoomPlayerReadyCheck(Room);		//	�濡 �ο��� ���� á���� �˻� �� ���� �÷��� ��ȯ �˻�

	return;
}

bool CPlayer::VersionCheck()
{
	//	Config�� ���� �ڵ�� �´��� �񱳸� �Ѵ�.
	if (_Version != Config.VER_CODE)
	{
		//	���� �ڵ尡 �ٸ� ��� �α׸� ����� �������� ������ ���� �� ���´�.
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

bool CPlayer::OverlappLoginCheck()
{
	//	�ߺ� �α��� �˻�
	if (true == Find_AccountNo(_AccountNo))
	{
		//	�ߺ��α��� ��Ŷ ����
		//	���� �α��� ������ �Լ� ���ο��� LogoutFlag ���� ó��
		CPacket *newPacket = CPacket::Alloc();
		WORD Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Result = OVERLAPP_LOGIN;
		*newPacket << Type << _AccountNo << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

void CPlayer::HttpJsonCall()
{
	//-------------------------------------------------------------
	//	JSON ������ - select ��û
	//-------------------------------------------------------------
	//	RingBuffer-�޸�Ǯ �����Ͽ� HttpQueue�� ������ �� �̺�Ʈ ȣ��
	WORD Type = SELECT;
	CRingBuffer *pBuffer = _pGameServer->_HttpPool->Alloc();
	pBuffer->Initialize(500);
	pBuffer->Enqueue((char*)&Type, sizeof(Type));
	pBuffer->Enqueue((char*)&_iArrayIndex, sizeof(_iArrayIndex));
	pBuffer->Enqueue((char*)&_AccountNo, sizeof(_AccountNo));
	_pGameServer->_HttpQueue.Enqueue(pBuffer);
	SetEvent(_pGameServer->_hHttpEvent);
	return;
}

bool CPlayer::AccountnoCheck(INT64 AccountNo)
{
	//	AccountNo�� ���� ���� �� �׽�Ʈ��
	if (AccountNo != _AccountNo)
	{
		//	������ȣ�� �ٸ� - �α� ����� ����
		_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"AccountNo Not Same [My AccountNo : %d, RecvAccountNo]"), _AccountNo, AccountNo);
		Disconnect();
		return false;
	}
	return true;
}

BATTLEROOM * CPlayer::FindWaitRoom(int RoomNo)
{
	//	WaitRoomMap���� �� ��ȣ�� ���� ã��
	bool NotFind = false;
	std::map<int, BATTLEROOM*>::iterator iter;
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	iter = _pGameServer->_WaitRoomMap.find(RoomNo);
	if (iter == _pGameServer->_WaitRoomMap.end())
		NotFind = true;
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	//	���� �����ϴ��� Ȯ�� �� ���� ���� ��� �� ���� ��Ŷ ����
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

bool CPlayer::WaitRoomCheck(bool RoomReady)
{
	//	�������� Ȯ��
	if (true == RoomReady)
	{
		//	������ �ƴ� ��Ŷ ����
		CPacket * newPacket = CPacket::Alloc();
		BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
		BYTE Result = NOT_READYROOM;
		WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket << Type << _AccountNo << _RoomNo << MaxUser << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

bool CPlayer::EnterTokenCheck(char * EnterToken, char * RoomToken)
{
	//	EnterToken ��ġ�� ��� ���� ���
	if (0 != strncmp(EnterToken, RoomToken, sizeof(EnterToken)))
	{
		//	�ٸ���� �α� ����� EnterToken �ٸ� ��Ŷ ����
		CPacket * newPacket = CPacket::Alloc();
		BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
		BYTE Result = ENTERTOKEN_FAIL;
		WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket << Type << _AccountNo << _RoomNo << MaxUser << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

bool CPlayer::WaitRoomUserNumCheck(BATTLEROOM * Room)
{
	//	�濡 ������ �� �� �ִ��� �ο��� üũ
	if (0 >= (Room->MaxUser - Room->CurUser))
	{
		//	�� �ڸ��� ���� ��Ŷ ����
		CPacket * newPacket = CPacket::Alloc();
		BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
		BYTE Result = ROOMUSER_MAX;
		WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket << Type << _AccountNo << _RoomNo << MaxUser << Result;
		SendPacket(newPacket);
		newPacket->Free();
		return false;
	}
	return true;
}

void CPlayer::RoomEnterSuccess(BATTLEROOM * Room)
{
	//	_RoomNo�� �� ��ȣ�� RoomPlayer ����ü�� Session Index��ȣ ����
	//	����� ���� ���� �ʾ����� ���� ������ ������ ���� �߰��Ͽ� �ذ�����
	//	_RoomNo = RoomNo;

	RoomPlayerInfo * pInfo = _pGameServer->_RoomPlayerPool->Alloc();
	pInfo->AccountNo = _AccountNo;
	pInfo->Index = _iArrayIndex;
	AcquireSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	Room->RoomPlayer.push_back(pInfo);
	ReleaseSRWLockExclusive(&_pGameServer->_WaitRoom_lock);
	Room->CurUser++;
	//	�� ���� ���� ��Ŷ ����
	CPacket * newPacket = CPacket::Alloc();
	BYTE MaxUser = Config.BATTLEROOM_MAX_USER;
	BYTE Result = enENTERROOM_RESULT::SUCCESS;
	WORD Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
	*newPacket << Type << _AccountNo << _RoomNo << MaxUser << Result;
	SendPacket(newPacket);
	newPacket->Free();
	return;
}

void CPlayer::RoomEnterPlayer(BATTLEROOM * Room)
{
	//	���������� ������ ������ ��� �濡 ������ �߰��� ��Ŷ�� �߰� ����
	//	���� ������ Ŭ���̾�Ʈ���Ե� ��Ŷ�� ������
	//	GameServer.cpp���� ��Ŷ�� �������� Index��ȣ�� �ʿ���
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
	//	���� ������ �÷��̾�� ���� ���� ���� ����
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
	//	���� �ο��� ���� ��á���� �˻� �� RoomReady �÷��� ��ȯ
	//	�÷��� �ٲٸ� Auth���� ���� �� �� ���� ��Ŷ ����
	//-----------------------------------------------------------
	if (0 == Room->MaxUser - Room->CurUser)
	{
		Room->RoomReady = true;
	}
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
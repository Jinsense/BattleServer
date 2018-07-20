#include "Player.h"

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
		*pPacket >> _AccountNo;
		pPacket->PopData((char*)_SessionKey, sizeof(_SessionKey));
		*pPacket >> _Version;

		//	Config�� ���� �ڵ�� �´��� �񱳸� �Ѵ�.
		if (_Version != Config.VER_CODE)
		{
			//	���� �ڵ尡 �ٸ� ��� �α׸� ����� �������� ������ ���� �� ���´�.
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
		//	�ߺ� �α��� �˻�
		if (true == Find_AccountNo(_AccountNo))
		{
			//	�ߺ��α��� ��Ŷ ����
			//	���� �α��� ������ �Լ� ���ο��� LogoutFlag ���� ó��
			CPacket *newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Result = OVERLAPP_LOGIN;
			*newPacket << Type << _AccountNo << Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;
		}		
		//-------------------------------------------------------------
		//	JSON ������ - select_account.php ��û - ����Ű ��
		//-------------------------------------------------------------
		//	Set Post Data
		Json::Value PostData;
		Json::StyledWriter writer;
		PostData["accountno"] = _AccountNo;
		string Data = writer.write(PostData);
		WinHttpClient HttpClient(Config.APISERVER_SELECT_ACCOUNT);
		HttpClient.SetAdditionalDataToSend((BYTE*)Data.c_str(), Data.size());
		//	Set Request Header
		wchar_t szSize[50] = L"";
		swprintf_s(szSize, L"%d", Data.size());
		wstring Headers = L"Content-Length: ";
		Headers += szSize;
		Headers += L"\r\nContent-Type: application/x-www-form-urlencoded\r\n";
		HttpClient.SetAdditionalRequestHeaders(Headers);
		//	Send HTTP post request
		HttpClient.SendHttpRequest(L"POST");
		//	Response wstring -> string convert
		wstring response = HttpClient.GetResponseContent();
		string temp;
		temp.assign(response.begin(), response.end());
		//	Result Check
		Json::Reader reader;
		Json::Value result;
		bool Res = reader.parse(temp, result);
		if (!Res)
		{
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Failed to parse Json [AccountNo : %d]"), _AccountNo);
			CPacket * newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Status = CLIENT_ERROR;
			*newPacket << Type << Status;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
			return;
		}
		int ResNum = result["result"].asInt();
		if (LOGIN_SUCCESS != ResNum)
		{
			BYTE Status = CLIENT_ERROR;
			CPacket * newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			*newPacket << Type << Status;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
			return;
		}

		string sessionkey = result["sessionkey"].asCString();
		if (0 != strncmp(sessionkey.c_str(), _SessionKey, sizeof(_SessionKey)))
		{
			//	����Ű�� �ٸ� ���� �� ����
			CPacket * newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Status = SESSIONKEY_ERROR;
			*newPacket << Type << Status;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
			return;
		}
		//	Nickname ���� - char�� ��ȯ �� wchar�� ����
		string Nickname = result["nickname"].asCString();
		char Temp[20] = { 0 , };
		strcpy_s(Temp, sizeof(Temp), Nickname.c_str());
		UTF8toUTF16(Temp, _Nickname, sizeof(Temp));		

		//	JSON ������ - select_contents.php ��û - ���� ���� �ҷ�����
		HttpClient.UpdateUrl(Config.APISERVER_SELECT_CONTENTS);
		HttpClient.SetAdditionalDataToSend((BYTE*)Data.c_str(), Data.size());
		//	Set Request Header
		wchar_t Size[50] = L"";
		swprintf_s(Size, L"%d", Data.size());
		Headers = L"Content-Length: ";
		Headers += Size;
		Headers += L"\r\nContent-Type: application/x-www-form-urlencoded\r\n";
		HttpClient.SetAdditionalRequestHeaders(Headers);
		//	Send HTTP post request
		HttpClient.SendHttpRequest(L"POST");
		//	Response wstring -> string convert
		response = HttpClient.GetResponseContent();
		temp.assign(response.begin(), response.end());

		Res = reader.parse(temp, result);
		if (!Res)
		{
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"Failed to parse Json [AccountNo : %d]"), _AccountNo);
			CPacket * newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Status = CLIENT_ERROR;
			*newPacket << Type << Status;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
			return;
		}
		int ResNum = result["result"].asInt();
		if (LOGIN_SUCCESS != ResNum)
		{
			BYTE Status = CLIENT_ERROR;
			CPacket * newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			*newPacket << Type << Status;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
			return;
		}
		_Playtime = result["play"].asInt();
		_Playcount = result["playercount"].asInt();
		_Kill = result["kill"].asInt();
		_Die = result["Die"].asInt();
		_Win = result["Win"].asInt();

		//	���� ��Ŷ ����
//		_AuthToGameFlag = true;

		CPacket *pNewPacket = CPacket::Alloc();
		Type = en_PACKET_CS_GAME_RES_LOGIN;
		BYTE Status = 1;

		*pNewPacket << Type << Status << _AccountNo;
		SendPacket(pNewPacket);
		pNewPacket->Free();
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
		INT64 AccountNo = NULL;
		int RoomNo = NULL;
		char EnterToken[32] = { 0, };
		*pPacket >> AccountNo >> RoomNo;
		pPacket->PopData((char*)&EnterToken, sizeof(EnterToken));
		//	Ư�� �� ���� ��û
		//	AccountNo�� ���� ���� �� �׽�Ʈ��
		if (AccountNo != _AccountNo)
		{
			//	������ȣ�� �ٸ� - �α� ����� ����
			_pLog->Log(const_cast<WCHAR*>(L"Error"), LOG_SYSTEM, const_cast<WCHAR*>(L"AccountNo Not Same [My AccountNo : %d, RecvAccountNo]"), _AccountNo, AccountNo);
			Disconnect();
		}
		//	EnterToken ��ġ�� ��� ���� ���
		if (0 != strncmp(EnterToken, _pBattleServer->_CurConnectToken, sizeof(_pBattleServer->_CurConnectToken)))
		{
			//	CurToken�� �ƴҰ�� OldToken �˻�
			if (0 != strncmp(EnterToken, _pBattleServer->_OldConnectToken, sizeof(_pBattleServer->_OldConnectToken)))
			{
				//	�Ѵ� �ƴҰ�� �α� ����� EnterToken �ٸ� ��Ŷ ����
				CPacket * newPacket = CPacket::Alloc();
				WORD MaxUser = Config.BATTLEROOM_MAX_USER;
				WORD Result = ENTERTOKEN_FAIL;
				Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
				*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
				SendPacket(newPacket);
				newPacket->Free();
				return;
			}
		}
		//	BattleRoomMap���� �� ��ȣ�� ���� ã��
		bool NotFind = false;
		bool NotReadyRoom = false;
		std::map<int, BATTLEROOM*>::iterator iter;
		AcquireSRWLockExclusive(&_pBattleServer->_BattleRoom_lock);
		iter = _pBattleServer->_BattleRoomMap.find(RoomNo);
		if (iter == _pBattleServer->_BattleRoomMap.end())
			NotFind = true;
		if (true == (*iter).second->RoomReady)
			NotReadyRoom = true;
		ReleaseSRWLockExclusive(&_pBattleServer->_BattleRoom_lock);
		//	���� �����ϴ��� Ȯ�� �� ���� ���� ��� �� ���� ��Ŷ ����
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
		//	�������� Ȯ��
		if (true == NotReadyRoom)
		{
			//	������ �ƴ� ��Ŷ ����
			CPacket * newPacket = CPacket::Alloc();
			WORD MaxUser = Config.BATTLEROOM_MAX_USER;
			WORD Result = NOT_READYROOM;
			Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
			*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
			SendPacket(newPacket);
			newPacket->Free();
			return;
		}
		//	�濡 ������ �� �� �ִ��� �ο��� üũ
		if (0 >= ((*iter).second->MaxUser - (*iter).second->CurUser))
		{
			//	�� �ڸ��� ���� ��Ŷ ����
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
		//	�濡 ���� �ο��� ��á�� ��� ������ ������ ��� �� ���� ��Ŷ ����
		if ((*iter).second->CurUser == (*iter).second->MaxUser)
		{
			CPacket * CloseRoomPacket = CPacket::Alloc();
			Type = en_PACKET_BAT_MAS_REQ_CLOSED_ROOM;
			*CloseRoomPacket >> Type >> (*iter).second->RoomNo;
			_pGameServer->_pMaster->SendPacket(CloseRoomPacket);
			CloseRoomPacket->Free();
			(*iter).second->RoomReady = true;
		}
		//	_RoomNo�� �� ��ȣ�� RoomPlayer ����ü�� Session Index��ȣ ����
		//	����� ���� ���� �ʾ����� ���� ������ ������ ���� �߰��Ͽ� �ذ�����
		_RoomNo = RoomNo;
		RoomPlayerInfo Info;
		Info.AccountNo = _AccountNo;
		Info.Index = _iArrayIndex;
		(*iter).second->RoomPlayer.push_back(Info);
		//	�� ���� ���� ��Ŷ ����
		CPacket * newPacket = CPacket::Alloc();
		WORD MaxUser = Config.BATTLEROOM_MAX_USER;
		WORD Result = enENTERROOM_RESULT::SUCCESS;
		Type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
		*newPacket >> AccountNo >> RoomNo >> MaxUser >> Result;
		SendPacket(newPacket);
		newPacket->Free();
		//	���������� ������ ������ ��� �濡 ������ �߰��� ��Ŷ�� �߰� ����
		//	���� ������ Ŭ���̾�Ʈ���Ե� ��Ŷ�� ������
		//	GameServer.cpp���� ��Ŷ�� �������� Index��ȣ�� �ʿ���
		CPacket * AddPacket = CPacket::Alloc();
		*AddPacket >> _RoomNo >> _AccountNo;
		AddPacket->PushData((char*)&_Nickname, sizeof(_Nickname));
		AddPacket->AddRef();
		*AddPacket >> _Playcount >> _Playtime >> _Kill >> _Die >> _Win;
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
	//	���������� ������ ���� ������ �غ� �� ���� ( ���� �ʿ� ĳ���� ����, ����Ʈ �غ� ��� )

	return;
}

void CPlayer::OnGame_ClientLeave()
{
	//	���� ���������� �÷��̾� ���� ( ����ʿ��� ����, ��Ƽ���� ��� )

	return;
}

void CPlayer::OnGame_Packet(CPacket *pPacket)
{
	// ���� ���� ��Ŷ ó��
	//== Game ������
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
	//	��Ŷ ó��
		
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
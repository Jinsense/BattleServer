#include "Player.h"

CPlayer::CPlayer()
{
	_AccountNo = NULL;
	ZeroMemory(&_SessionKey, sizeof(_SessionKey));
	_Version = NULL;
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
		//	Ư�� �� ���� ��û
		//	AccountNo�� ���� ���� �� �׽�Ʈ��
		//	EnterToken ��ġ�� ��� ���� ���

		//	�� ���� ���� ��Ŷ ����

		//	���������� ������ ������ ��� �濡 ������ �߰��� ��Ŷ�� �߰� ����
		//	���� ������ Ŭ���̾�Ʈ���Ե� ��Ŷ�� ������

		//	�濡 ���� �ο��� ��á�� ��� ������ ������ ��� �� ���� ��Ŷ ����

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
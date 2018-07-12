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
		//	JSON 데이터 - select_account.php 요청 - 세션키 비교
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
			//	세션키가 다름 응답 후 끊기
			CPacket * newPacket = CPacket::Alloc();
			Type = en_PACKET_CS_GAME_RES_LOGIN;
			BYTE Status = SESSIONKEY_ERROR;
			*newPacket << Type << Status;
			SendPacket(newPacket);
			//			SendPacketAndDisConnect(pPlayer->_ClientID, newPacket);
			newPacket->Free();
			return;
		}
		//	JSON 데이터 - select_contents.php 요청 - 계정 정보 불러오기
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

		//	성공 패킷 응답
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
		//	특정 방 입장 요청
		//	AccountNo는 버그 감지 및 테스트용
		//	EnterToken 일치할 경우 입장 허용

		//	방 입장 응답 패킷 전송

		//	정상적으로 입장이 성공할 경우 방에 유저가 추가됨 패킷도 추가 전송
		//	새로 접속한 클라이언트에게도 패킷을 보내줌

		//	방에 입장 인원이 꽉찼을 경우 마스터 서버로 대기 방 닫힘 패킷 전송

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
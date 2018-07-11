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
		bool Find = false;
		//	�ߺ� �α������� AccountNo üũ - Map �˻� �� Map �߰�
		AcquireSRWLockExclusive(&_pBattleServer->_AccountNoMap_srwlock);
		if (_pBattleServer->_AccountNoMap.end() == _pBattleServer->_AccountNoMap.find(_AccountNo))
		{
			//	Not Found
			Find = false;
		}
		else
		{
			//	Found - �ߺ� �α���
			Find = true;
		}
		ReleaseSRWLockExclusive(&_pBattleServer->_AccountNoMap_srwlock);
		if (false == Find)
		{
			_pBattleServer->_AccountNoMap.insert(std::make_pair(_AccountNo, _pBattleServer->_pSessionArray[_iArrayIndex]));
		}
		else
		{
			
		}
		//	JSON ������ - select_account.php ��û - ����Ű ��


		//	JSON ������ - select_contents.php ��û - ���� ���� �ҷ�����


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
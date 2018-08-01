#ifndef _BATTLESERVER_SERVER_DEFINE_H_
#define _BATTLESERVER_SERVER_DEFINE_H_

#include <Windows.h>
#include <list>

#define		RINGBUFFERSIZE		3000

enum enENTERROOM_RESULT
{
	SUCCESS = 1,
	ENTERTOKEN_FAIL = 2,
	NOT_READYROOM = 3,
	NOT_FINDROOM = 4,
	ROOMUSER_MAX = 5,
};

enum enHTTPTYPE
{
	SELECT = 1,
	UPDATE = 2,

};
typedef struct st_RoomPlayerInfo
{
	UINT64 AccountNo;
	int  Index;
}RoomPlayerInfo;

typedef struct st_BattleRoom
{
	char Entertoken[32] = { 0, };	//	Entertoken;
	int RoomNo;			//	�� ��ȣ
	int MaxUser;		//	�ִ� ����
	int CurUser;		//	���� ����
	__int64 ReadyCount;	//	���� �غ�Ϸ� �ð�
	bool RoomReady;		//	���� �غ�Ϸ� �÷���
	bool PlayReady;		//	�����غ� �Ϸ� �÷���
	bool GameEnd;		//	�ش� ���� ������ �������� ����
	std::list<RoomPlayerInfo*> RoomPlayer;		//	�濡 �ִ� ���� ���
}BATTLEROOM;

#endif
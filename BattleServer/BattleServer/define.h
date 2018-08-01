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
	int RoomNo;			//	방 번호
	int MaxUser;		//	최대 유저
	int CurUser;		//	현재 유저
	__int64 ReadyCount;	//	대기방 준비완료 시간
	bool RoomReady;		//	대기방 준비완료 플래그
	bool PlayReady;		//	게임준비 완료 플래그
	bool GameEnd;		//	해당 방의 게임이 끝났는지 여부
	std::list<RoomPlayerInfo*> RoomPlayer;		//	방에 있는 유저 목록
}BATTLEROOM;

#endif
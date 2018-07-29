#ifndef _BATTLESERVER_SERVER_LANCLIENT_H_
#define _BATTLESERVER_SERVER_LANCLIENT_H_


#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#include "Packet.h"
#include "RingBuffer.h"
#include "LockFreeQueue.h"
#include "CommonProtocol.h"

#define LANCLIENT_WORKERTHREAD	2
#define LANCLIENT_WSABUFNUM		100
#define LANCLIENT_QUEUESIZE		20000

#define LANCLIENT_HEADERSIZE	2

typedef struct st_LanClient
{
	bool		bConnect;
	long		SendFlag;
	long		Send_Count;
	long		IO_Count;
	OVERLAPPED				SendOver, RecvOver;
	CRingBuffer				RecvQ, PacketQ;
	CLockFreeQueue<CPacket*> SendQ;
	SOCKET					sock;

	st_LanClient() :
		RecvQ(LANCLIENT_QUEUESIZE),
		PacketQ(LANCLIENT_QUEUESIZE),
		SendFlag(false),
		bConnect(false),
		IO_Count(0) {}
}LANCLIENTSESSION;


#endif

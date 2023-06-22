#pragma once
//#include <Windows.h>
//
//#include "CPacket.h"
//#include "RingBuffer.h"
//#include "LockFreeQueue.h"
//#include "Tracer.h"
//#define TRACE_SESSION

#define MAX_SENDQ 200


#ifdef TRACE_SESSION
enum
{
	enDisconnect = 0,
	enSendFailed = 1,
	enSendAsync = 2,
	enSendSync = 3,
	enSendPacket = 7,
	enAccept = 8,
	enCancelIO = 9,
	enLeave = 10,
	enRecvFailed = 11,
	enRecvAsync = 12,
	enRecvSync = 13,
	enPost = 20,
	enSendResult = 21,
	enRecvResult = 22,
	enReleasePend = 98,
	enRelease,

};
#endif

class alignas(64) Session
{
	friend class CNetServer;
	friend class CLanServer;
	friend class CLanClient;

private:

	Session();
	~Session();

	unsigned long long GetSessionID();

#ifndef STACK_INDEX
	bool used;
#endif

	alignas(64) unsigned int session_id;
	unsigned short session_index;


	OVERLAPPED recv_overlapped;

	OVERLAPPED send_overlapped;
	int recvPacket;
	int sendPacket;
	RingBuffer recv_buffer = RingBuffer(2000);
#ifdef AUTO_PACKET
	LockFreeQueue<PacketPtr> send_q = LockFreeQueue<PacketPtr>(MAX_SENDQ, false);
#else
	LockFreeQueue<CPacket*> send_q = LockFreeQueue<CPacket*>(MAX_SENDQ, false);
#endif

	RingBuffer send_buffer = RingBuffer(4000);

	// interlock
	alignas(64) SOCKET sock;
	alignas(64) int io_count; //(session ref count) 경계에만 세우고 뒤에 다른 변수 들어올 수 있음.
	int release_flag;
	alignas(64) int pend_count; // CancelIO 타이밍 잡기
	int disconnect;
	alignas(64) unsigned int send_post_flag;
	//alignas(64) unsigned int send_pend_flag;
	alignas(64) ULONG64 last_recv_time;

	wchar_t ip[16];
	unsigned short port;

#ifdef TRACE_SESSION
	MiniTracer pending_tracer;
#endif

	// 락프리 스택쓰기?
#ifdef AUTO_PACKET
	PacketPtr temp_packet[200];
#else

#endif

};

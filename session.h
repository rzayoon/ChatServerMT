#pragma once
#include <Windows.h>
#include "CPacket.h"
#include "RingBuffer.h"
#include "LockFreeQueue.h"
#include "Tracer.h"

//#define TRACE_SESSION

class alignas(64) Session
{
public:

	Session();
	~Session();

	void Lock();
	void Unlock();

#ifndef STACK_INDEX
	bool used;
#endif

	alignas(64) unsigned int session_id;
	unsigned short session_index;

	LARGE_INTEGER send_time;

	OVERLAPPED recv_overlapped;
	SOCKET recv_sock;
	DWORD recvbytes;
	OVERLAPPED send_overlapped;
	SOCKET send_sock;
	DWORD sendbytes;
	RingBuffer recv_q = RingBuffer(2000);
#ifdef AUTO_PACKET
	LockFreeQueue<PacketPtr> send_q = LockFreeQueue<PacketPtr>(0, TRUE);
#else
	LockFreeQueue<CPacket*> send_q = LockFreeQueue<CPacket*>(50, false);
#endif

	// interlock
	alignas(64) SOCKET sock;
	alignas(64) int io_count; //(session ref count) ��迡�� ����� �ڿ� �ٸ� ���� ���� �� ����.
	int release_flag;
	alignas(64) int pend_count; // CancelIO Ÿ�̹� ���
	int disconnect;
	alignas(64) int send_flag;
	alignas(64) int send_packet_cnt;  // Send�� ���� Packet ��ü ������ �ʿ�
	alignas(64) DWORD ref_time;
	ULONGLONG send_packet_time;

	wchar_t ip[16];
	unsigned short port;
	CRITICAL_SECTION session_cs;

#ifdef TRACE_SESSION
	MiniTracer pending_tracer;
#endif

#ifdef AUTO_PACKET
	PacketPtr temp_packet[200];
#else
	CPacket* temp_packet[200];
#endif

};

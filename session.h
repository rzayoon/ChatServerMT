#pragma once
#include <Windows.h>
#include "CPacket.h"
#include "RingBuffer.h"
#include "LockFreeQueue.h"

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

	bool disconnect;  
	// 64����Ʈ�� ����..? disconnect �ٲ� id, index�� �״�� �о����..
	alignas(8) unsigned int session_id;
	unsigned short session_index;

	LARGE_INTEGER send_time;

	OVERLAPPED recv_overlapped;
	SOCKET recv_sock;
	OVERLAPPED send_overlapped;
	SOCKET send_sock;
	RingBuffer recv_q = RingBuffer(2000);
#ifdef AUTO_PACKET
	LockFreeQueue<PacketPtr> send_q = LockFreeQueue<PacketPtr>(0, TRUE);
#else
	LockFreeQueue<CPacket*> send_q = LockFreeQueue<CPacket*>(0);
#endif

	// interlock
	alignas(64) SOCKET sock;
	alignas(64) int io_count; // ��迡�� ����� �ڿ� �ٸ� ���� ���� �� ����.
	int release_flag;
	alignas(64) int send_flag;
	alignas(64) int send_packet_cnt;  // Send�� ���� Packet ��ü ������ �ʿ�
	alignas(64) int b;

	wchar_t ip[16];
	unsigned short port;
	CRITICAL_SECTION session_cs;

#ifdef AUTO_PACKET
	PacketPtr temp_packet[200];
#else
	CPacket* temp_packet[200];
#endif

};

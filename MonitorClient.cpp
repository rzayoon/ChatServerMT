#include <Windows.h>

#include "CPacket.h"
#include "Tracer.h"
#include "RingBuffer.h"

#include "LockFreeQueue.h"

#include "CLanClient.h"
#include "MonitorClient.h"

#include "CommonProtocol.h"



MonitorClient::MonitorClient()
{

}

MonitorClient::~MonitorClient()
{

}

void MonitorClient::OnEnterServerJoin()
{
	CPacket* send_packet = CPacket::Alloc();
	WORD type = en_PACKET_SS_MONITOR_LOGIN;
	int no = 3; // Chat Server = 3

	*send_packet << type << no;

	SendPacket(send_packet);


	CPacket::Free(send_packet);
	return;

}

void MonitorClient::OnLeaveServer()
{

}

void MonitorClient::OnWorkerThreadBegin()
{

}

void MonitorClient::OnWorkerThreadEnd()
{

}

void MonitorClient::OnRecv(CPacket* packet)
{

}

void MonitorClient::OnError(int errorcode, const wchar_t* msg)
{

}

void MonitorClient::SendMonitorData(BYTE data_type, int data_val, int time_stamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	CPacket* send_packet = CPacket::Alloc();
	*send_packet << type << data_type << data_val << time_stamp;

	SendPacket(send_packet);

	CPacket::Free(send_packet);

	return;
}

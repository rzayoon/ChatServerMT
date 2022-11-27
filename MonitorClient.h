#pragma once
#include "CNetClient.h"
#include "MonitorPacket.h"

class MonitorClient : public CNetClient
{
public:

	MonitorClient();
	virtual ~MonitorClient();

	void OnEnterServerJoin();

	void OnLeaveServer();

//	void OnSend(int send_size);

	void OnWorkerThreadBegin();

	void OnWorkerThreadEnd();

	void OnRecv(CPacket* packet);

	void OnError(int errorcode, const wchar_t* msg);

public:
	CPacket* AllocPacket() { return MonitorPacket::Alloc(); }
	void FreePacket(CPacket* packet) { MonitorPacket::Free(packet); }
	void SetPacket(unsigned char code, unsigned char key)
	{
		MonitorPacket::SetPacketCode(code);
		MonitorPacket::SetPacketKey(key);

	}

	int GetUsePool()
	{
		return MonitorPacket::packet_pool.GetUseSize();
	}

};

extern MonitorClient g_monitorCli;
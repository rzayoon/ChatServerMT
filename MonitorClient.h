#pragma once
#include "CNetClient.h"

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


};

extern MonitorClient g_monitorCli;
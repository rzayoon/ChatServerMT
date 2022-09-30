#pragma once
#include "CNetServer.h"



class ChatServer : public CNetServer
{
public:
	ChatServer();
	virtual ~ChatServer();


private:
	bool OnConnectionRequest(wchar_t* ip, unsigned short port);
	void OnClientJoin(unsigned long long session_id);
	void OnClientLeave(unsigned long long session_id);

	void OnRecv(unsigned long long session_id, CPacket* packet);

	void OnSend(unsigned long long session_id, int send_size);

	void OnWorkerThreadBegin();
	void OnWorkerThreadEnd();

	void OnError(int errorcode, const wchar_t* msg);


	HANDLE m_hSingleThread;


};

extern ChatServer g_server;
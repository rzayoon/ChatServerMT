#pragma once
#include <Windows.h>

#include "CPacket.h"
#include "session.h"
#include "Tracer.h"
#include "monitor.h"

#define STACK_INDEX


class CNetServer
{
	enum {
		ID_MASK = 0xFFFFFFFF,	
		INDEX_BIT_SHIFT = 32,
		MAX_WSABUF = 100
	};

public:

	CNetServer()
	{
		ZeroMemory(ip, sizeof(ip));
	}

	~CNetServer()
	{
		if(isRunning)
			Stop();
	}

	bool Start(
		const wchar_t* ip, unsigned short port, 
		int iocp_worker, int iocp_active, int max_session, int max_user, 
		unsigned char packet_key, unsigned char packet_code);
	void Stop();

	void DisconnectSession(unsigned long long session_id);
	int GetSessionCount();


#ifdef AUTO_PACKET
	bool SendPacket(unsigned long long session_id, PacketPtr packet);
	virtual void OnRecv(unsigned long long session_id, PacketPtr packet) = 0;
#else
	bool SendPacket(unsigned long long session_id, CPacket* packet);
	virtual void OnRecv(unsigned long long session_id, CPacket* packet) = 0;
#endif

	virtual bool OnConnectionRequest(wchar_t* ip, unsigned short port) = 0;
	virtual void OnClientJoin(unsigned long long session_id/**/) = 0;
	virtual void OnClientLeave(unsigned long long session_id) = 0;
	
	virtual void OnSend(unsigned long long session_id, int send_size) = 0;


	virtual void OnWorkerThreadBegin() = 0;
	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, const wchar_t* msg) = 0;
	
	void Show();

private:


	HANDLE hcp;

	HANDLE hAcceptThread;
	HANDLE* hWorkerThread;
	int iocp_worker;
	int iocp_active;
	int max_session;

protected:
	unsigned int max_user;
	
private:
	bool nagle; 

	// packet
	unsigned char packet_key;
	unsigned char packet_code;
	// Server IP Port
	unsigned short port;
	wchar_t ip[16];

	static unsigned long _stdcall AcceptThread(void* param);
	static unsigned long _stdcall IoThread(void* param);

	void RunAcceptThread();
	void RunIoThread();

	bool RecvPost(Session* session);
	bool SendPost(Session* session);

	// libary ³»ºÎ¿ë
	void Disconnect(Session* session);

	int UpdateIOCount(Session* session);
	void ReleaseSession(Session* session);

	bool exit_flag = false;
	bool isRunning = false;

#ifdef STACK_INDEX
	LockFreeStack<unsigned short> empty_session_stack;
#endif

	Session* session_arr;
	unsigned int m_sess_id = 1;
	
	Monitor monitor;
	Tracer tracer;

	alignas(64) int session_cnt = 0;
};


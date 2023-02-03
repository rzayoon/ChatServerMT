#pragma once
#include "MonitorClient.h"
#include "CCpuUsage.h"
#include "CPDH.h"


#define dfUSER_MAP_HASH 1

//#define dfTRACE_CHAT

class ChatServer : public CNetServer
{
	friend class PacketProcessor;
public:
	ChatServer();
	virtual ~ChatServer();

	bool Start();
	void Stop();


private:
	bool OnConnectionRequest(const wchar_t* ip, unsigned short port);
	void OnClientJoin(unsigned long long session_id);
	void OnClientLeave(unsigned long long session_id);

	void OnRecv(unsigned long long session_id, CPacket* packet);

	void OnSend(unsigned long long session_id, int send_size);

	void OnWorkerThreadBegin();
	void OnWorkerThreadEnd();

	void OnError(int errorcode, const wchar_t* msg);


private:

	bool AcquireUser(SS_ID s_id, User** user);
	void ReleaseUser(User* user);
	void CreateUser(SS_ID s_id);
	void DeleteUser(SS_ID s_id);

	void SendMessageUni(CPacket* packet, User* user);
	void SendMessageSector(CPacket* packet, int sector_x, int sector_y, User* user);
	void SendMessageAround(CPacket* packet, User* user);

	MemoryPoolTls<User> m_userPool;

	unordered_map<SS_ID, User*> m_userMap[dfUSER_MAP_HASH];
	SRWLOCK m_userMapCS[dfUSER_MAP_HASH];
	
	unordered_map<long long, SS_ID> m_accountMap;
	SRWLOCK m_accountMapSRW;

private:

	alignas(64) unsigned int m_connectCnt;
	alignas(64) unsigned int m_loginCnt;
	alignas(64) unsigned int m_duplicateLogin;
	alignas(64) unsigned int m_messageTps;
#ifdef dfTRACE_CHAT
	Tracer m_chatTracer;
#endif
public:
	void Show();
	void CheckTimeOut();


private:

	HANDLE h_timeOutThread;
	alignas(64) bool m_runTimeCheck;

	MonitorClient m_monitorCli;
	CCpuUsage m_CpuTime;
	CPDH m_pdh;

	unsigned int m_collectMsgTPS;

	wchar_t m_monitorIP[16];
	unsigned short m_monitorPort;
	int m_monitorWorker;
	int m_monitorActive;

public:

	void Collect();

	bool IsConnectedMonitor()
	{
		return m_monitorCli.IsConnected();
	}
	bool ConnectMonitor();

	void SendMonitor(int time_stamp);

};

DWORD TimeOutThread(PVOID param);

extern ChatServer g_chatServer;
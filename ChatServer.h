#pragma once





#define dfUSER_MAP_HASH 1

//#define dfTRACE_CHAT

class ChatServer : public CNetServer
{
	friend class PacketProcessor;

public:
	ChatServer();
	virtual ~ChatServer();


private:
	bool OnConnectionRequest(const wchar_t* ip, unsigned short port);
	void OnClientJoin(unsigned long long session_id);
	void OnClientLeave(unsigned long long session_id);

	void OnRecv(unsigned long long session_id, CPacket* packet);

	void OnSend(unsigned long long session_id, int send_size);

	void OnClientTimeout(unsigned long long session_id);

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



private:


	MonitorClient m_monitorCli;
	CCpuUsage m_CpuTime;
	CPDH m_pdh;

	unsigned int m_collectMsgTPS;


public:

	void Collect();

	bool IsConnectedMonitor()
	{
		return m_monitorCli.IsConnected();
	}
	bool ConnectMonitor();

	void SendMonitor(int time_stamp);

	void SetMonitorClientInfo(const wchar_t* serverIp, unsigned short port);
	void SetRedisInfo(const char* ip, unsigned short port);

private:

	wchar_t m_monitorIP[16];
	unsigned short m_monitorPort;

	cpp_redis::client m_redis;
	char m_redisIP[16];
	unsigned short m_redisPort;

public:

	void ConnectRedis()
	{
		m_redis.connect(m_redisIP, m_redisPort);
	}

};


extern ChatServer g_chatServer;
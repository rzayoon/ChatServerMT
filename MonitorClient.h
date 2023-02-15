#pragma once
class MonitorClient : public CLanClient
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

	bool IsConnected()
	{
		return m_isConnected;
	}

	void SendMonitorData(BYTE data_type, int data_val, int time_stamp);


};


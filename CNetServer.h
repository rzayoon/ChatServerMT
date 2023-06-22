#pragma once



//#include <Windows.h>
//#include "CPacket.h"
//#include "session.h"
//#include "Tracer.h"
//#include "CrashDump.h"

#define SEND_ZEROCOPY
#define SYNC_IO_MONITOR


/// <summary>
/// 
/// �� Ŭ������ ����Ͽ� �����.
/// ��� �� ���������� ȣ���� �Լ��� 
/// Start(), Stop(), DisconnectSession(), SendPacket()
/// GetSessionCount(), Show()
/// 
/// </summary>

class CPacket;
class Session;
class Tracer;
template <class T>
class LockFreeStack;


class CNetServer
{

	enum class ePost {
		SEND_PEND = 1,
		RELEASE_PEND = 2,
		CANCEL_IO = 3
	};

public:

	CNetServer();

	~CNetServer();

	/// <summary>
	/// ���� �ʱ�ȭ �� ����
	/// </summary>
	/// <param name="ip"> ���� ������ </param>
	/// <param name="port"> ���� ��Ʈ </param>
	/// <param name="iocp_worker"> ������ worker ������ �� </param>
	/// <param name="iocp_active"> IOCP�� ���� �ִ� ������ �� </param>
	/// <param name="max_session"> Accept ���� �ִ� ���� �� </param>
	/// <param name="max_user"> �������� �Ѱ��� �ִ� ���� �� </param>
	/// <param name="packet_key"> Packet Encoding Ű </param>
	/// <param name="packet_code"> Packet Encoding �ڵ� </param>
	/// <returns>���� �� false ��ȯ</returns>
	bool Start(
		const wchar_t* ip, unsigned short port, 
		int iocp_worker, int iocp_active, int max_session, bool nagle, 
		unsigned char packet_key, unsigned char packet_code);

	/// <summary>
	/// ���� ���� ������ ����
	/// </summary>
	void Stop();

	/// <summary>
	/// Session ���� ����. Recv�� Send ���� ���� ��� ��ġ�� ������.
	/// </summary>
	/// <param name="session_id"></param>
	void DisconnectSession(unsigned long long session_id);
	


#ifdef AUTO_PACKET
	bool SendPacket(unsigned long long session_id, PacketPtr packet);
	virtual void OnRecv(unsigned long long session_id, PacketPtr packet) = 0;
#else
	/// <summary>
	/// Session�� Packet ����, ���� �� ���ο��� Packet�� ref count ������Ŵ.
	/// </summary>
	/// <param name="session_id"> ��� session Id </param>
	/// <param name="packet"> ������ ��Ŷ ������ </param>
	/// <returns> ���� ���� session�� ��� false ��ȯ </returns>
	bool SendPacket(unsigned long long session_id, CPacket* packet);

	/// <summary>
	/// Recv ���� Packet�� ���� ó�� ����
	/// </summary>
	/// <param name="session_id"> ��� session Id </param>
	/// <param name="packet"> ���� Packet ������ </param>
	virtual void OnRecv(unsigned long long session_id, CPacket* packet) = 0;
#endif

	/// <summary>
	/// Accept�� Ŭ���̾�Ʈ�� IP, Port ����. 
	/// �ʿ� ���׿� ���� false return �� Session���� ���� �ʴ´�.
	/// </summary>
	/// <param name="ip"> ���ӵ� IP </param>
	/// <param name="port"> ���ӵ� Port </param>
	/// <returns> �ʿ信 ���� boolean ���� </returns>
	virtual bool OnConnectionRequest(const wchar_t* ip, unsigned short port) = 0;

	/// <summary>
	/// ���������� ���� ó���� Session�� Id ����
	/// </summary>
	/// <param name="session_id"></param>
	virtual void OnClientJoin(unsigned long long session_id/**/) = 0;

	/// <summary>
	/// ���� ������ Session Id ����.
	/// Release�� ���� �ȵǾ��� �� ����
	/// </summary>
	/// <param name="session_id"></param>
	virtual void OnClientLeave(unsigned long long session_id) = 0;
	
	virtual void OnClientTimeout(unsigned long long session_id) = 0;
	/// <summary>
	/// ��û�� ���� ó���� �Ϸ�� Send ���� ��� Session Id�� ���� ũ��
	/// </summary>
	virtual void OnSend(unsigned long long session_id, int send_size) = 0;

	/// <summary>
	/// IOCP�� ���� ��Ŀ ������ ��� �� ȣ��. �������� ��� ó���� ��쿡 ȣ��ȴ�.
	/// </summary>
	virtual void OnWorkerThreadBegin() = 0;
	/// <summary>
	/// ��� ó�� �Ϸ� �� �����尡 ��⿡ ���� �� ȣ��
	/// </summary>
	virtual void OnWorkerThreadEnd() = 0;

	/// <summary>
	/// ���� ��Ȳ�� ���� �˸�
	/// </summary>
	/// <param name="errorcode"></param>
	/// <param name="msg"></param>
	virtual void OnError(int errorcode, const wchar_t* msg) = 0;
	
	/// <summary>
	/// ���̺귯�� Monitoring ���� ���
	/// </summary>
	void Show();

	int GetSessionCount() { return m_sessionCnt; }

private:

	HANDLE m_hcp;
	HANDLE m_hAcceptThread;
	HANDLE m_hTimeOutThread;
	HANDLE* m_hWorkerThread;
	int m_iocpWorkerNum;
	int m_iocpActiveNum;
	int m_maxSession;

protected:
	// �������� �Ѱ��� session ��
	unsigned int m_maxUser;
	
private:
	bool m_nagle; 

	// packet
	unsigned char m_packetKey;
	unsigned char m_packetCode;
	// Server IP Port
	unsigned short m_port;
	wchar_t m_ip[16];

	static unsigned long _stdcall acceptThread(void* param);
	static unsigned long _stdcall ioThread(void* param);
	static unsigned long _stdcall timeoutThread(void* param);

	void runAcceptThread();
	void runIoThread();
	void runTimeoutThread();

	bool recvPost(Session* session);
	void sendPost(Session* session);

	void sendPend(Session* session);

	void trySend(Session* session);
	
	void disconnect(Session* session);
	int updateIOCount(Session* session);
	void releasePend(Session* session);
	void release(Session* session);

	bool m_isRunning;

	LockFreeStack<unsigned short>* m_emptySessionStack;

	Session* m_sessionArr;
	unsigned int m_sessID = 1;


#ifdef TRACE_SERVER
	Tracer tracer;
#endif

	alignas(64) unsigned long long m_totalAccept = 0;
	alignas(64) unsigned int m_sessionCnt = 0;
	alignas(64) long m_syncRecv = 0;
	alignas(64) long m_syncSend = 0;
	alignas(64) long m_recvByte = 0;
	alignas(64) long m_sendByte = 0;
	alignas(64) long m_sendPacket = 0;
	alignas(64) long m_recvPacket = 0;
	alignas(64) unsigned long long m_maxTransferred = 0;
	alignas(64) LONG m_semiTimeOut = 0;
	alignas(64) LONG m_AbortedByLocal = 0;
	unsigned long long m_preAccept = 0;
	int m_acceptErr = 0;

};


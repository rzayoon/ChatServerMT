#pragma once
#include <Windows.h>

#include "CPacket.h"
#include "session.h"
#include "Tracer.h"
#include "monitor.h"



/// <summary>
/// 
/// �� Ŭ������ ����Ͽ� �����.
/// ��� �� ���������� ȣ���� �Լ��� 
/// Start(), Stop(), DisconnectSession(), SendPacket()
/// GetSessionCount(), Show()
/// 
/// </summary>
class CNetServer
{
	enum {
		ID_MASK = 0xFFFFFFFF,	
		INDEX_BIT_SHIFT = 32,
		MAX_WSABUF = 10
	};

public:

	CNetServer()
	{
		m_isRunning = false;
		ZeroMemory(m_ip, sizeof(m_ip));
	}

	~CNetServer()
	{
		if(m_isRunning)
			Stop();
	}

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
	int GetSessionCount();


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
	virtual bool OnConnectionRequest(wchar_t* ip, unsigned short port) = 0;

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

private:


	HANDLE m_hcp;
	HANDLE m_hAcceptThread;
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

	static unsigned long _stdcall AcceptThread(void* param);
	static unsigned long _stdcall IoThread(void* param);

	void RunAcceptThread();
	void RunIoThread();

	bool RecvPost(Session* session);
	void SendPost(Session* session);

	
	void Disconnect(Session* session);
	int UpdateIOCount(Session* session);
	void UpdatePendCount(Session* session);
	void CancelIOSession(Session* session);
	void ReleaseSession(Session* session);
	void LeaveSession(Session* session);


	bool m_isRunning;

#ifdef STACK_INDEX
	LockFreeStack<unsigned short> empty_session_stack;
#endif

	Session* m_sessionArr;
	unsigned int m_sess_id = 1;
	
	Monitor monitor;
#ifdef TRACE_SERVER
	Tracer tracer;
#endif

	alignas(64) int m_sessionCnt = 0;
};


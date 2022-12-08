#pragma once
#include <Windows.h>

#include "CPacket.h"
#include "Tracer.h"
#include "RingBuffer.h"

#define SEND_ZEROCOPY

#define MAX_SENDQ 200

/// <summary>
/// 
/// �� Ŭ������ ����Ͽ� �����.
/// ��� �� ���������� ȣ���� �Լ��� 
/// Start(), Stop(), DisconnectSession(), SendPacket()
/// GetSessionCount(), Show()
/// 
/// </summary>
class CLanClient
{
	enum {
		ID_MASK = 0xFFFFFFFF,	
		INDEX_BIT_SHIFT = 32,
		MAX_WSABUF = 30
	};

public:

	CLanClient()
	{
		m_isConnected = false;
		ZeroMemory(m_serverIp, sizeof(m_serverIp));
		
		m_isInit = false;
		m_serverSock = 0;
		m_disconnect = true;
		m_ioCount = 0;
		m_sendFlag = false;
		m_sentPacketCnt = 0;
	}

	~CLanClient();

	bool Connect(
		const wchar_t* ip, unsigned short port, 
		int iocp_worker, int iocp_active, bool nagle);

	void Disconnect();



	bool SendPacket(CPacket* packet);

	void Show();

	virtual void OnRecv(CPacket* packet) = 0;


	virtual void OnEnterServerJoin() = 0;

	/// <summary>
	/// ���� ������ Session Id ����.
	/// Release�� ���� �ȵǾ��� �� ����
	/// </summary>
	/// <param name="session_id"></param>
	virtual void OnLeaveServer() = 0;
	
	/// <summary>
	/// ��û�� ���� ó���� �Ϸ�� Send ���� ��� Session Id�� ���� ũ��
	/// </summary>
	//virtual void OnSend(int send_size) = 0;

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
	

protected:

	bool m_isConnected;

private:

	bool m_isInit;
	void ReleaseClient(void);

	HANDLE m_hcp;
	HANDLE* m_hWorkerThread;
	int m_iocpWorkerNum;
	int m_iocpActiveNum;

private:
	bool m_nagle; 

	// packet
	unsigned char m_packetKey;
	unsigned char m_packetCode;
	// Server IP Port
	unsigned short m_serverPort;
	wchar_t m_serverIp[16];

	static unsigned long _stdcall IoThread(void* param);

	void RunIoThread();

	bool RecvPost();
	void SendPost();

	bool ConnectSession();
	void DisconnectSession();
	int UpdateIOCount();
	void UpdatePendCount();
	void CancelIOPend();
	void Release();
	void Leave();




	//Session m_session;
	SOCKET m_serverSock;
	
	OVERLAPPED m_recvOverlapped;
	OVERLAPPED m_sendOverlapped;

	RingBuffer m_recvQ = RingBuffer(2000);

	LockFreeQueue<CPacket*> m_sendQ = LockFreeQueue<CPacket*>(MAX_SENDQ, false);

	alignas(64) unsigned int m_sessionId;

	alignas(64) int m_ioCount;
	int m_releaseFlag;
	alignas(64) int m_pendCount; // CancelIO Ÿ�̹� ���
	int m_disconnect;
	alignas(64) int m_leaveFlag;
	alignas(64) int m_sendFlag;
	alignas(64) int m_sentPacketCnt;  // Send�� ���� Packet ��ü ������ �ʿ�
	alignas(64) char _b;

	CPacket* m_sentPacket[200];
};


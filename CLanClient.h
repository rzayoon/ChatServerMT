#pragma once
//#include <Windows.h>
//
//#include "MemoryPoolTls.h"
//
//#include "RingBuffer.h"
//#include "CPacket.h"
//#include "Tracer.h"

#define SEND_ZEROCOPY

#define MAX_SENDQ 200

/// <summary>
/// 
/// 이 클래스는 상속하여 사용함.
/// 사용 시 직접적으로 호출할 함수는 
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

	enum class ePost {
		SEND_PEND = 1,
		RELEASE_PEND = 2,
		CANCEL_IO = 3
	};


public:

	CLanClient();

	virtual ~CLanClient();

	bool Connect(
		const wchar_t* ip, unsigned short port, 
		int iocp_worker, int iocp_active, bool nagle);

	void Disconnect();
	bool SendPacket(CPacket* packet);
	void Show();

	virtual void OnRecv(CPacket* packet) = 0;
	virtual void OnEnterServerJoin() = 0;

	/// <summary>
	/// 연결 끊어진 Session Id 전달.
	/// 
	/// </summary>
	/// <param name="session_id"></param>
	virtual void OnLeaveServer() = 0;
	
	/// <summary>
	/// 요청에 대한 처리가 완료된 Send 건의 대상 Session Id와 보낸 크기
	/// </summary>
	//virtual void OnSend(int send_size) = 0;

	/// <summary>
	/// IOCP에 의해 워커 스레드 깨어날 때 호출. 정상적인 결과 처리일 경우에 호출된다.
	/// </summary>
	virtual void OnWorkerThreadBegin() = 0;
	/// <summary>
	/// 결과 처리 완료 후 스레드가 대기에 들어가기 전 호출
	/// </summary>
	virtual void OnWorkerThreadEnd() = 0;

	/// <summary>
	/// 에러 상황에 대한 알림
	/// </summary>
	/// <param name="errorcode"></param>
	/// <param name="msg"></param>
	virtual void OnError(int errorcode, const wchar_t* msg) = 0;
	

protected:

	bool m_isConnected;

private:

	bool m_isInit;
	void releaseClient(void);

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

	static unsigned long _stdcall ioThread(void* param);

	void runIoThread();

	bool recvPost();
	void sendPost();
	void sendPend();

	void trySend();

	bool connectSession();
	void disconnectSession();

	int updateIOCount();

	void releasePend();
	void release();


	SOCKET m_serverSock;
	
	OVERLAPPED m_recvOverlapped;
	OVERLAPPED m_sendOverlapped;

	RingBuffer m_recvQ = RingBuffer(2000);

	LockFreeQueue<CPacket*> m_sendQ = LockFreeQueue<CPacket*>(MAX_SENDQ, false);

	alignas(64) unsigned int m_sessionId;

	alignas(64) int m_ioCount;
	int m_releaseFlag;
	alignas(64) int m_disconnect;
	alignas(64) int m_sendFlag;
	alignas(64) int m_sentPacketCnt;  // Send에 넣은 Packet 객체 삭제에 필요
	alignas(64) char _b;

	CPacket* m_sentPacket[200];
};


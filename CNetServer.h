#pragma once
#include <Windows.h>

#include "CPacket.h"
#include "session.h"
#include "Tracer.h"
#include "monitor.h"



/// <summary>
/// 
/// 이 클래스는 상속하여 사용함.
/// 사용 시 직접적으로 호출할 함수는 
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
	/// 서버 초기화 및 동작
	/// </summary>
	/// <param name="ip"> 서버 아이피 </param>
	/// <param name="port"> 서버 포트 </param>
	/// <param name="iocp_worker"> 생성할 worker 스레드 수 </param>
	/// <param name="iocp_active"> IOCP가 깨울 최대 스레드 수 </param>
	/// <param name="max_session"> Accept 받을 최대 세션 수 </param>
	/// <param name="max_user"> 컨텐츠에 넘겨줄 최대 유저 수 </param>
	/// <param name="packet_key"> Packet Encoding 키 </param>
	/// <param name="packet_code"> Packet Encoding 코드 </param>
	/// <returns>실패 시 false 반환</returns>
	bool Start(
		const wchar_t* ip, unsigned short port, 
		int iocp_worker, int iocp_active, int max_session, bool nagle, 
		unsigned char packet_key, unsigned char packet_code);

	/// <summary>
	/// 서버 중지 스레드 정리
	/// </summary>
	void Stop();

	/// <summary>
	/// Session 연결 끊음. Recv나 Send 동작 중일 경우 마치고 정리됨.
	/// </summary>
	/// <param name="session_id"></param>
	void DisconnectSession(unsigned long long session_id);
	int GetSessionCount();


#ifdef AUTO_PACKET
	bool SendPacket(unsigned long long session_id, PacketPtr packet);
	virtual void OnRecv(unsigned long long session_id, PacketPtr packet) = 0;
#else
	/// <summary>
	/// Session에 Packet 전송, 성공 시 내부에서 Packet의 ref count 증가시킴.
	/// </summary>
	/// <param name="session_id"> 대상 session Id </param>
	/// <param name="packet"> 전송할 패킷 포인터 </param>
	/// <returns> 연결 끊긴 session일 경우 false 반환 </returns>
	bool SendPacket(unsigned long long session_id, CPacket* packet);

	/// <summary>
	/// Recv 받은 Packet에 대한 처리 로직
	/// </summary>
	/// <param name="session_id"> 대상 session Id </param>
	/// <param name="packet"> 받은 Packet 포인터 </param>
	virtual void OnRecv(unsigned long long session_id, CPacket* packet) = 0;
#endif

	/// <summary>
	/// Accept된 클라이언트의 IP, Port 전달. 
	/// 필요 사항에 따라 false return 시 Session으로 받지 않는다.
	/// </summary>
	/// <param name="ip"> 접속된 IP </param>
	/// <param name="port"> 접속된 Port </param>
	/// <returns> 필요에 따라 boolean 리턴 </returns>
	virtual bool OnConnectionRequest(wchar_t* ip, unsigned short port) = 0;

	/// <summary>
	/// 성공적으로 접속 처리된 Session의 Id 전달
	/// </summary>
	/// <param name="session_id"></param>
	virtual void OnClientJoin(unsigned long long session_id/**/) = 0;

	/// <summary>
	/// 연결 끊어진 Session Id 전달.
	/// Release는 아직 안되었을 수 있음
	/// </summary>
	/// <param name="session_id"></param>
	virtual void OnClientLeave(unsigned long long session_id) = 0;
	
	/// <summary>
	/// 요청에 대한 처리가 완료된 Send 건의 대상 Session Id와 보낸 크기
	/// </summary>
	virtual void OnSend(unsigned long long session_id, int send_size) = 0;

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
	
	/// <summary>
	/// 라이브러리 Monitoring 정보 출력
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
	// 컨텐츠에 넘겨줄 session 수
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


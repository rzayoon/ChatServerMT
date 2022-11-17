#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdio.h>

#include "LockFreeQueue.h"
#include "CNetServer.h"
#include "NetProtocol.h"
#include "CrashDump.h"

long long packet_counter[101];
int log_arr[100];

bool CNetServer::Start(const wchar_t* ip, unsigned short port,
	int iocpWorker, int iocpActive, int maxSession, bool nagle,
	unsigned char packetKey, unsigned char packetCode)
{
	if (m_isRunning)
	{
		OnError(90, L"Duplicate Start Request\n");
		return false;
	}

	m_nagle = nagle;

	m_maxSession = maxSession;
	wcscpy_s(m_ip, ip);
	m_port = port;

	m_iocpActiveNum = iocpActive;
	m_iocpWorkerNum = iocpWorker;

	m_packetKey = packetKey;
	m_packetCode = packetCode;

	CPacket::SetPacketCode(m_packetCode);
	CPacket::SetPacketKey(m_packetKey);



	// WinSock 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		OnError(1, L"WSAStartup()\n");
		return false;
	}

	// IOCP 생성
	m_hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_iocpActiveNum);
	if (m_hcp == NULL)
	{
		OnError(2, L"Create IOCP()\n");
		return false;
	}

	// Accept Thread 생성
	m_hAcceptThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AcceptThread, this, CREATE_SUSPENDED, NULL);
	if (m_hAcceptThread == NULL)
	{
		OnError(3, L"Create Thread Failed\n");
		return false;
	}

	// Worker Thread 생성
	m_hWorkerThread = new HANDLE[m_iocpWorkerNum];
	for (int i = 0; i < m_iocpWorkerNum; i++)
	{
		m_hWorkerThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IoThread, this, CREATE_SUSPENDED, NULL);
		if (m_hWorkerThread[i] == NULL)
		{
			OnError(3, L"Create Thread Failed\n");
			return false;
		}
	}


	m_sessionArr = new Session[m_maxSession];

#ifdef STACK_INDEX
	for (int i = m_maxSession - 1; i >= 0; i--)
		empty_session_stack.Push(i);
#endif
	
	ResumeThread(m_hAcceptThread);
	for (int i = 0; i < m_iocpWorkerNum; i++)
	{
		ResumeThread(m_hWorkerThread[i]);
	}

	m_isRunning = true;

	return true;
}

void CNetServer::Stop()
{
	if (!m_isRunning)
	{
		OnError(98, L"Not Running Yet\n");
		return;
	}

	m_isRunning = false;

	
	// blocking된 Accept 깨우기
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		wprintf(L"exit socket fail\n");
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	wchar_t ip[16] = L"127.0.0.1";  //loop back

	InetPtonW(AF_INET, ip, &serveraddr.sin_addr.s_addr);

	serveraddr.sin_port = htons(m_port);

	int ret_con = connect(sock, (sockaddr*)&serveraddr, sizeof(serveraddr));
	if (ret_con == SOCKET_ERROR)
	{
		DWORD error_code = WSAGetLastError();
		wprintf(L"exit connect fail %d\n", error_code);
	}

	closesocket(sock);

	// session 연결 끊기
	for (int i = 0; i < m_maxSession; i++)
	{
		if (!m_sessionArr[i].release_flag)
		{
			DisconnectSession(*(unsigned long long*) & m_sessionArr[i].session_id);
		}
	}
	
	// session 릴리즈될 때까지 대기
	while (m_sessionCnt != 0)
	{
		Sleep(0); 
	}

	wprintf(L"Disconnected all session\n");


	HANDLE* hExit = new HANDLE[static_cast<__int64>(m_iocpWorkerNum) + 1];
	for (int i = 0; i < m_iocpWorkerNum; i++)
	{
		hExit[i] = m_hWorkerThread[i];
	}
	delete[] m_hWorkerThread;
	hExit[m_iocpWorkerNum] = m_hAcceptThread;

	// worker 종료 메시지 
	// PostQueuedCompletionStatus 0 0 0
	for (int i = 0; i < m_iocpWorkerNum; i++)
		PostQueuedCompletionStatus(m_hcp, 0, 0, 0);

	WaitForMultipleObjects(m_iocpWorkerNum + 1, hExit, TRUE, INFINITE);

	delete[] hExit;
	delete[] m_sessionArr;


	WSACleanup();

	

	return;
}

inline int CNetServer::GetSessionCount()
{
	return m_sessionCnt;
}

unsigned long _stdcall CNetServer::AcceptThread(void* param)
{
	CNetServer* server = (CNetServer*)param;
	wprintf(L"%d Accept thread On...\n", GetCurrentThreadId());
	server->RunAcceptThread();
	// this call이나 server-> 콜이나 mov 2회지만 코딩 편의상
	// mov reg1 [this]/[server(지역주소)]
	// mov reg2 [reg1]
	wprintf(L"%d Accept thread end\n", GetCurrentThreadId());
	return 0;
}

unsigned long _stdcall CNetServer::IoThread(void* param)
{
	CNetServer* server = (CNetServer*)param;
	wprintf(L"%d worker thread On...\n", GetCurrentThreadId());
	server->RunIoThread();
	wprintf(L"%d IO thread end\n", GetCurrentThreadId());
	return 0;
}

inline void CNetServer::RunAcceptThread()
{
	int retVal;

	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
		OnError(4, L"Listen socket()\n");
		return;
	}

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	InetPtonW(AF_INET, m_ip, &serveraddr.sin_addr.s_addr);
	serveraddr.sin_port = htons(m_port);

	retVal = bind(listenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retVal == SOCKET_ERROR) return;

	// 소켓 송신 버퍼
	int sendBufSize = 0;
	setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));

	// nagle
	if (m_nagle)
		setsockopt(listenSock, IPPROTO_TCP, TCP_NODELAY, (char*)&m_nagle, sizeof(m_nagle));

	// RST로 종료
	LINGER linger;
	linger.l_linger = 0;
	linger.l_onoff = true;
	setsockopt(listenSock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));




	retVal = listen(listenSock, SOMAXCONN);
	if (retVal == SOCKET_ERROR) return;

	wprintf_s(L"Listen Port: %d\n", m_port);

	SOCKET clientSock;
	SOCKADDR_IN clientAddr;
	int addrLen;

	wchar_t temp_ip[16];
	unsigned short temp_port;

	while (m_isRunning)
	{
		addrLen = sizeof(clientAddr);
		clientSock = accept(listenSock, (SOCKADDR*)&clientAddr, &addrLen);
		if (clientSock == INVALID_SOCKET)
		{
			monitor.IncAcceptErr();
			continue;
		}
		InetNtopW(AF_INET, &clientAddr.sin_addr, temp_ip, _countof(temp_ip));
		temp_port = ntohs(clientAddr.sin_port);
#ifdef TRACE_SERVER
		tracer.trace(70, 0, clientSock);
#endif
		monitor.IncAccept();

		if (OnConnectionRequest(temp_ip, temp_port))
		{
			unsigned short index;
#ifdef STACK_INDEX
			if (!empty_session_stack.Pop(&index))
			{
				closesocket(clientSock);
				continue;
			}
#else
			bool find = false;
			for (index = 0; index < _max_client; index++)
			{
				if (seesion_arr[index].used == false)
				{
					find = true;
					break;
				}
			}

			if (!find)
			{
				closesocket(clientSock);
				continue;
			}
			session_arr[index].used = true;
#endif

			Session* session = &m_sessionArr[index];

			InterlockedIncrement((LONG*)&session->io_count);

#ifdef TRACE_SESSION
			session->pending_tracer.trace(enAccept, clientSock);
#endif
			session->session_id = m_sess_id++;
			if (m_sess_id == 0) m_sess_id++;
			*(unsigned*)&session->session_index = index;
			session->sock = clientSock;
			wcscpy_s(session->ip, _countof(session->ip), temp_ip);
			session->port = ntohs(clientAddr.sin_port);
			session->send_flag = false;
			session->send_packet_cnt = 0;
			session->leave_flag = false;
			session->disconnect = false;
			// send queue는 release 때 정리되어 있어야 함
			//session->send_q.ClearBuffer();
			session->recv_q.ClearBuffer();

			CreateIoCompletionPort((HANDLE)session->sock, m_hcp, (ULONG_PTR)session, 0);

			session->release_flag = 0; // 준비 끝

			//tracer.trace(10, session, session->session_id); // accept

			//접속
			InterlockedIncrement((LONG*)&m_sessionCnt);

			// 접속에 대한 처리 먼저 해야 함.
			OnClientJoin(*((unsigned long long*) & session->session_id));
			
			RecvPost(session);

			UpdateIOCount(session);

		}
		else // Connection Requeset 거부
		{
			closesocket(clientSock);
		}
	}
}


inline void CNetServer::RunIoThread()
{
	int ret_gqcp;
	DWORD thread_id = GetCurrentThreadId();
	LARGE_INTEGER recv_start, recv_end;
	LARGE_INTEGER send_start, send_end;
	LARGE_INTEGER on_recv_st, on_recv_ed;
	DWORD error_code;



	while (1)
	{
		DWORD cbTransferred;
		WSAOVERLAPPED* overlapped;
		Session* session;
		ret_gqcp = GetQueuedCompletionStatus(m_hcp, &cbTransferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE); // overlapped가 null인지 확인 우선

		OnWorkerThreadBegin();
		if (overlapped == NULL) // deque 실패 1. timeout 2. 잘못 호출(Invalid handle) 3. 임의로 queueing 한 것(PostQueue)
		{
			if (session == nullptr) 
			{
				break;
			}
		}


		if (ret_gqcp == 0)
		{
			//에러코드 로깅
			error_code = GetLastError();
			if (error_code != ERROR_NETNAME_DELETED) {
#ifdef TRACE_SERVER
				tracer.trace(00, session, error_code);
#endif
				OnError(error_code, L"GQCS return 0");
			}
		}

		session->ref_time = GetTickCount64();

		if (cbTransferred == 0 || session->disconnect) // Pending 후 I/O 처리 실패
		{
#ifdef TRACE_SERVER
			tracer.trace(78, session, session->session_id);
#endif
			if (!session->leave_flag)
			{
				LeaveSession(session);
			}

		}
		else {
			if (&session->recv_overlapped == overlapped) // recv 결과 처리
			{
				if (session->recv_sock != session->sock)
					log_arr[2]++;
				//tracer.trace(21, session, session->session_id);
				QueryPerformanceCounter(&recv_start);


				session->recv_q.MoveRear(cbTransferred);
				// msg 확인

				while (true)
				{
					NetPacketHeader header;

					if (session->recv_q.Peek((char*)&header, sizeof(header)) != sizeof(header))
						break;

					int q_size = session->recv_q.GetFillSize();
					if (header.len + sizeof(header) > q_size)
						break;

#ifdef AUTO_PACKET
					PacketPtr packet = CPacket::Alloc();
					int ret_deq = session->recv_q.Dequeue((*packet)->GetBufferPtr(), header.len);
					(*packet)->MoveWritePos(ret_deq);
					(*packet)->Decode();

					QueryPerformanceCounter(&on_recv_st);
					OnRecv(*(unsigned long long*) & session->session_id, packet);
					QueryPerformanceCounter(&on_recv_ed);
					monitor.AddOnRecvTime(&on_recv_st, &on_recv_ed);
#else
					CPacket* packet = CPacket::Alloc();
					int ret_deq = session->recv_q.Dequeue(packet->GetBufferPtrNet(), header.len + sizeof(header));
					packet->MoveWritePos(header.len);
					if (!packet->Decode())
					{
						Disconnect(session);
					}

					QueryPerformanceCounter(&on_recv_st);
					OnRecv(*(unsigned long long*) & session->session_id, packet);
					QueryPerformanceCounter(&on_recv_ed);
					monitor.AddOnRecvTime(&on_recv_st, &on_recv_ed);

					CPacket::Free(packet);
#endif

				}
				QueryPerformanceCounter(&recv_end);
				monitor.AddRecvCompTime(&recv_start, &recv_end);
				monitor.IncRecv();

				//tracer.trace(22, session, session->session_id);
				bool ret_recv = RecvPost(session);

			}
			else if (&session->send_overlapped == overlapped) // send 결과 처리
			{
				QueryPerformanceCounter(&send_start);
				monitor.AddSendToComp(&session->send_time, &send_start);

				OnSend(*(unsigned long long*) & session->session_id, cbTransferred);
				if (session->send_sock != session->sock)
					CrashDump::Crash();
				//tracer.trace(31, session, session->session_id);

				monitor.UpdateSendPacket(cbTransferred);

				int packet_cnt = session->send_packet_cnt;
				if (packet_cnt == 0)
				{
					CrashDump::Crash();
				}
#ifdef AUTO_PACKET				
				while (packet_cnt > 0)
				{
					session->temp_packet[--packet_cnt].~PacketPtr();
				}


#else
				while (packet_cnt > 0)
				{
					monitor.IncSendPacket(); // 실제로 보낸 Packet 수
					session->temp_packet[--packet_cnt]->SubRef();
				}
#endif

				session->send_packet_cnt = 0;
				session->send_flag = false;

				QueryPerformanceCounter(&send_end);
				monitor.AddSendCompTime(&send_start, &send_end);

				//tracer.trace(32, session, session->session_id);
				if (session->send_q.GetSize() > 0)
					SendPost(session);

			}
			else // send, recv 다 아님(다른 session의 overlapped 전달)
			{
				CrashDump::Crash();
			}
		}

		UpdateIOCount(session);
		OnWorkerThreadEnd();
	}

	return;
}

#ifdef AUTO_PACKET
bool CNetServer::SendPacket(unsigned long long session_id, PacketPtr packet)
{
	monitor.IncSendPacket();
	bool ret = false;

	unsigned short idx = session_id >> INDEX_BIT_SHIFT;
	unsigned int id = session_id & ID_MASK;

#ifndef STACK_INDEX

	for (idx = 0; idx < max_session; idx++)
	{
		if (session_arr[idx].session_id == id)
			break;
	}

#endif

	Session* session = &session_arr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
	if (session->release_flag == 0)
	{
		if (session->session_id == id)
		{
			session->send_q.Enqueue(packet);  // 64 bit 기준 8byte

			SendPost(session);

			ret = true;
		}
		else
		{
			log_arr[5]++;
		}
	}
	UpdateIOCount(session);




	/*if (!ret)
	{
		monitor.IncNoSession();
	}*/

	return ret;
}
#else
bool CNetServer::SendPacket(unsigned long long session_id, CPacket* packet)
{
	bool ret = false;

	unsigned short idx = session_id >> INDEX_BIT_SHIFT;
	unsigned int id = session_id & ID_MASK;

	if (idx >= m_maxSession)
		CrashDump::Crash();

#ifndef STACK_INDEX

	for (idx = 0; idx < max_session; idx++)
	{
		if (session_arr[idx].session_id == id)
			break;
	}

#endif

	Session* session = &m_sessionArr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
#ifdef TRACE_SESSION
	session->pending_tracer.trace(enSendPacket, 0, GetTickCount64());
#endif
	if (session->release_flag == 0 && !session->disconnect)
	{
		if (session->session_id == id)
		{
			packet->AddRef();

			if (session->send_q.Enqueue(packet))
			{
				session->send_packet_time = GetTickCount64();
				SendPost(session);

				ret = true;
			}
			else
			{
				packet->SubRef();
			}
		}
		else
		{
			log_arr[5]++;
		}
	}
	UpdateIOCount(session);




	if (!ret)   // 필요하지 않은듯
	{
		monitor.IncNoSession();
	}

	return ret;
}
#endif


inline void CNetServer::DisconnectSession(unsigned long long session_id)
{
	unsigned short idx = session_id >> INDEX_BIT_SHIFT;
	unsigned int id = session_id & ID_MASK;

	if (idx >= m_maxSession)
		CrashDump::Crash();

#ifndef STACK_INDEX

	for (idx = 0; idx < max_session; idx++)
	{
		if (session_arr[idx].session_id == id)
			break;
	}

#endif

	Session* session = &m_sessionArr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
	if (session->release_flag == 0)
	{
		if (session->session_id == id)
		{
			Disconnect(session);
		}
	}
	UpdateIOCount(session);

	return;
}

inline void CNetServer::Disconnect(Session* session)
{
	//InterlockedIncrement((LONG*)&session->io_count);
	if (session->disconnect == 0)
	{
		if (InterlockedCompareExchange((LONG*)&session->disconnect, 1, 0) == 0) {
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enDisconnect, session->sock, GetTickCount64());
#endif
			// pending cnt == 0 이면 cancel IO 
			
			CancelIOSession(session);

		}

	}
	//UpdateIOCount(session);

	return;
}

inline bool CNetServer::RecvPost(Session* session)
{
	DWORD recvbytes, flags = 0;
	bool ret = false;

	int temp_pend = InterlockedIncrement((LONG*)&session->pend_count);
	if (session->disconnect == 0)
	{
		InterlockedIncrement((LONG*)&session->io_count);
		ZeroMemory(&session->recv_overlapped, sizeof(session->recv_overlapped));

		int emptySize = session->recv_q.GetEmptySize();
		int size1 = session->recv_q.DirectEnqueSize();

		WSABUF wsabuf[2];
		int cnt = 1;
		wsabuf[0].buf = session->recv_q.GetRearPtr();
		wsabuf[0].len = size1;

		if (size1 < emptySize)
		{
			++cnt;
			wsabuf[1].buf = session->recv_q.GetBufPtr();
			wsabuf[1].len = emptySize - size1;
		}


		SOCKET socket = session->sock;
		session->recv_sock = socket;
		DWORD error_code;

		int retval = WSARecv(socket, wsabuf, cnt, NULL, &flags, &session->recv_overlapped, NULL);

		if (retval == SOCKET_ERROR)
		{
			if ((error_code = WSAGetLastError()) != ERROR_IO_PENDING)
			{ // 요청이 실패
				LeaveSession(session);
				int io_temp = UpdateIOCount(session);
#ifdef TRACE_SERVER
				tracer.trace(1, session, error_code, socket);
#endif
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enRecvFailed, error_code, socket, GetTickCount64());
#endif
			}
			else
			{
				//tracer.trace(73, session, socket);
				// Pending
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enRecvAsync, error_code, socket, GetTickCount64());
#endif
				ret = true;
			}
		}
		else
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enRecvSync, 0, socket, GetTickCount64());
#endif
			//tracer.trace(73, session, socket);
			//동기 recv
			ret = true;
		}
	}
	UpdatePendCount(session);

	return ret;
}

inline void CNetServer::SendPost(Session* session)
{
	LARGE_INTEGER start, end;

	int temp_pend = InterlockedIncrement((LONG*)&session->pend_count);
	if (session->disconnect == 0)
	{

		if ((InterlockedExchange((LONG*)&session->send_flag, true)) == false) // compare exchange
		{
			long long buf_cnt = session->send_q.GetSize();
			if (buf_cnt <= 0)
			{
				log_arr[8]++;
				session->send_flag = false;
			}
			else
			{
				int temp_io = InterlockedIncrement((LONG*)&session->io_count);
				int retval;

				ZeroMemory(&session->send_overlapped, sizeof(session->send_overlapped));

				// 개선 필요
				if (buf_cnt > MAX_WSABUF)
					buf_cnt = MAX_WSABUF;

				WSABUF wsabuf[MAX_WSABUF];
				ZeroMemory(wsabuf, sizeof(wsabuf));

#ifdef AUTO_PACKET
				PacketPtr packet;
				for (int cnt = 0; cnt < buf_cnt;)
				{
					if (session->send_q.Dequeue(&packet) == false) continue;


					wsabuf[cnt].buf = (*packet)->GetBufferPtrNet();
					wsabuf[cnt].len = (*packet)->GetDataSizeNet();
					session->temp_packet[cnt] = packet;

					++cnt;
				}
#else
				CPacket* packet;
				for (int cnt = 0; cnt < buf_cnt;)
				{
					if (!session->send_q.Dequeue(&packet)) continue;

					wsabuf[cnt].buf = packet->GetBufferPtrNet();
					wsabuf[cnt].len = packet->GetDataSizeNet();
					session->temp_packet[cnt] = packet;

					++cnt;
				}
#endif

				session->send_packet_cnt = buf_cnt;
				DWORD sendbytes;
				monitor.IncSend();

				SOCKET socket = session->sock;
				session->send_sock = socket;
				QueryPerformanceCounter(&session->send_time);
				retval = WSASend(socket, wsabuf, buf_cnt, NULL, 0, &session->send_overlapped, NULL);
				QueryPerformanceCounter(&end);
				monitor.AddSendTime(&session->send_time, &end);

				DWORD error_code;
				if (retval == SOCKET_ERROR)
				{
					if ((error_code = WSAGetLastError()) != WSA_IO_PENDING) // 요청 자체가 실패
					{
						// 내가 release 시켜야하는 경우 Packet 해제 해줘야 함
						
						int io_temp = UpdateIOCount(session);
#ifdef TRACE_SERVER
						tracer.trace(2, session, error_code, socket);
#endif
#ifdef TRACE_SESSION
						session->pending_tracer.trace(enSendFailed, error_code, socket, GetTickCount64());
#endif
					}
					else
					{
						//tracer.trace(72, session, socket);
						// Pending
#ifdef TRACE_SESSION
						session->pending_tracer.trace(enSendAsync, error_code, socket, GetTickCount64());
#endif
					}
				}
				else
				{
					//동기처리
#ifdef TRACE_SESSION
					session->pending_tracer.trace(enSendSync, 0, socket, GetTickCount64());
#endif
				}
			}
		}
	}
	UpdatePendCount(session);


	return;
}

inline int CNetServer::UpdateIOCount(Session* session)
{
	int temp;
	//tracer.trace(76, session, session->session_id);
	if ((temp = InterlockedDecrement((LONG*)&session->io_count)) == 0)
	{
		ReleaseSession(session);
	}
	else if (temp < 0)
	{
		CrashDump::Crash();
	}

	return temp;
}

inline void CNetServer::UpdatePendCount(Session* session)
{
	// disconnect 한번에 확인
	int temp;
	if ((temp = InterlockedDecrement((LONG*)&session->pend_count)) == 0)
	{
		CancelIOSession(session);
	}

	return;

}

void CNetServer::CancelIOSession(Session* session)
{
	unsigned long long flag = *((unsigned long long*)(&session->pend_count));
	if (flag == 0x100000000)
	{
		if (InterlockedCompareExchange64((LONG64*)&session->pend_count, 0x200000000, flag) == flag)
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enCancelIO, session->sock, GetTickCount64());
#endif
			InterlockedIncrement((LONG*)&session->io_count);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)session, NULL);
			CancelIoEx((HANDLE)session->sock, NULL);
		}
	}

	return;
}


inline void CNetServer::ReleaseSession(Session* session)
{

	unsigned long long flag = *((unsigned long long*)(&session->io_count));
	if (flag == 0)
	{
		if (InterlockedCompareExchange64((LONG64*)&session->io_count, 0x100000000, flag) == flag)
		{
#ifdef TRACE_SERVER
			tracer.trace(75, session, session->session_id, session->sock);
#endif
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enRelease, session->sock);
#endif

			if (!session->leave_flag) CrashDump::Crash(); // 삭제 처리 확인
			
			

			session->session_id = 0;


#ifdef AUTO_PACKET
			PacketPtr packet;
			while (session->send_q.Dequeue(&packet))
			{
			}
			int remain = session->send_packet_cnt;
			while (remain > 0)
			{
				session->temp_packet[--remain].~PacketPtr();
			}

#else
			CPacket* packet;
			while (session->send_q.Dequeue(&packet))
			{
				packet->SubRef();
			}

			int remain = session->send_packet_cnt;
			while (remain > 0)
			{
				session->temp_packet[--remain]->SubRef();
			}
#endif

			closesocket(session->sock);
			session->sock = INVALID_SOCKET;



#ifdef STACK_INDEX
			empty_session_stack.Push(session->session_index);
#else
			session->used = false;
#endif
			InterlockedDecrement((LONG*)&m_sessionCnt);
		}
	}

	return;
}

void CNetServer::LeaveSession(Session* session)
{
	unsigned int flag = *((unsigned int*)(&session->leave_flag));
	if (flag == 0x0)
	{
		if (InterlockedCompareExchange((LONG*)&session->leave_flag, 0x1, flag) == flag)
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enLeave, session->sock, GetTickCount64());
#endif
			OnClientLeave(*(unsigned long long*) & session->session_id);
			
		}
	}

	return;
}


void CNetServer::Show()
{
	monitor.Show(m_sessionCnt, CPacket::GetUsePool(), 0);
	return;
}

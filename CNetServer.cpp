#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include "CLog.h"
#include "CNetServer.h"
#include "CPacket.h"
#include "CrashDump.h"
#include "LockFreeQueue.h"
#include "LockFreeStack.h"
#include "MemoryPoolTls.h"
#include "NetProtocol.h"
#include "RingBuffer.h"
#include "session.h"
#include "Tracer.h"

constexpr unsigned ID_MASK = 0xFFFFFFFF;
constexpr unsigned INDEX_BIT_SHIFT = 32;
constexpr int MAX_WSABUF = 2;




CNetServer::CNetServer()
{
	m_isRunning = false;
	ZeroMemory(m_ip, sizeof(m_ip));

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		CrashDump::Crash();
	}
}

CNetServer::~CNetServer()
{
	if (m_isRunning)
		Stop();

	WSACleanup();
}

bool CNetServer::Start(const wchar_t* ip, unsigned short port,
	int iocpWorker, int iocpActive, int maxSession, bool nagle,
	unsigned char packetKey, unsigned char packetCode)
{
	if (m_isRunning)
	{
		OnError(90, L"Duplicate Start Request\n");
		return false;
	}

	CLog::SYSLOG_Init(L"Log", enLOG_LEVEL_DEBUG);

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

	m_isRunning = true;



	// IOCP 생성
	m_hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_iocpActiveNum);
	if (m_hcp == NULL)
	{
		CrashDump::Crash();
	}

	// Accept Thread
	m_hAcceptThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)acceptThread, this, CREATE_SUSPENDED, NULL);
	if (m_hAcceptThread == NULL)
	{
		CrashDump::Crash();
	}
	
	// Time out thread
	m_hTimeOutThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)timeoutThread, this, 0, NULL);
	if (m_hTimeOutThread == NULL)
	{
		CrashDump::Crash();
	}


	// Worker Thread 생성
	m_hWorkerThread = new HANDLE[m_iocpWorkerNum];
	for (int i = 0; i < m_iocpWorkerNum; i++)
	{
		m_hWorkerThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ioThread, this, CREATE_SUSPENDED, NULL);
		if (m_hWorkerThread[i] == NULL)
		{
			CrashDump::Crash();
		}
	}


	m_sessionArr = new Session[m_maxSession];

	m_emptySessionStack = new LockFreeStack<unsigned short>(m_maxSession);
	for (int i = m_maxSession - 1; i >= 0; i--)
		m_emptySessionStack->Push(i);

	
	ResumeThread(m_hAcceptThread);
	for (int i = 0; i < m_iocpWorkerNum; i++)
	{
		ResumeThread(m_hWorkerThread[i]);
	}

	

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

	
	// blocking Accept 종료 처리 유도.
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

	// session 종료 요청
	for (int i = 0; i < m_maxSession; i++)
	{
		if (!m_sessionArr[i].release_flag)
		{
			DisconnectSession(*(unsigned long long*) & m_sessionArr[i].session_id);
		}
	}
	
	// session release 대기
	while (m_sessionCnt != 0)
	{
		Sleep(0); 
	}

	wprintf(L"Disconnected all session\n");


	HANDLE* hExit = new HANDLE[static_cast<__int64>(m_iocpWorkerNum) + 2];
	for (int i = 0; i < m_iocpWorkerNum; i++)
	{
		hExit[i] = m_hWorkerThread[i];
	}
	delete[] m_hWorkerThread;
	hExit[m_iocpWorkerNum++] = m_hAcceptThread;
	hExit[m_iocpWorkerNum++] = m_hTimeOutThread;

	// worker 종료 유도
	// PostQueuedCompletionStatus 0 0 0
	for (int i = 0; i < m_iocpWorkerNum - 2; i++)
		PostQueuedCompletionStatus(m_hcp, 0, 0, 0);

	WaitForMultipleObjects(m_iocpWorkerNum, hExit, TRUE, INFINITE);

	delete[] hExit;
	delete[] m_sessionArr;
	delete m_emptySessionStack;

	CloseHandle(m_hcp);


	return;
}





unsigned long _stdcall CNetServer::acceptThread(void* param)
{
	CNetServer* server = (CNetServer*)param;
	wprintf(L"%d Accept thread On...\n", GetCurrentThreadId());
	server->runAcceptThread();
	// this call과 포인터 call 차이 없음 코딩 편의상 멤버함수 호출
	// mov reg1 [this]/[server]
	// mov reg2 [reg1]
	wprintf(L"%d Accept thread end\n", GetCurrentThreadId());
	return 0;
}

unsigned long _stdcall CNetServer::ioThread(void* param)
{
	CNetServer* server = (CNetServer*)param;
	wprintf(L"%d worker thread On...\n", GetCurrentThreadId());
	server->runIoThread();
	wprintf(L"%d IO thread end\n", GetCurrentThreadId());
	return 0;
}

unsigned long _stdcall CNetServer::timeoutThread(void* param)
{
	CNetServer* server = (CNetServer*)param;

	server->runTimeoutThread();

	return 0;

}



void CNetServer::runAcceptThread()
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

	// listen 소켓의 송신 버퍼 크기가 accept된 소켓에 동일하게 설정된다.
#ifdef SEND_ZEROCOPY
	int sendBufSize = 0;
	setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
#endif

	/*int recvBufSize = 0;
	setsockopt(listenSock, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize));*/

	// nagle 알고리즘 파서에서 적용
	setsockopt(listenSock, IPPROTO_TCP, TCP_NODELAY, (char*)&m_nagle, sizeof(m_nagle));


	// RST 
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

	// 블로킹 Accept 동작
	while (m_isRunning)
	{
		addrLen = sizeof(clientAddr);
		clientSock = accept(listenSock, (SOCKADDR*)&clientAddr, &addrLen);
		if (clientSock == INVALID_SOCKET)
		{

			m_acceptErr++;

			continue;
		}
		InetNtopW(AF_INET, &clientAddr.sin_addr, temp_ip, _countof(temp_ip));
		temp_port = ntohs(clientAddr.sin_port);
#ifdef TRACE_SERVER
		tracer.trace(70, 0, clientSock);
#endif

		m_totalAccept++;

		// 화이트리스트 OR 블랙리스트 설정 가능
		if (OnConnectionRequest(temp_ip, temp_port))
		{
			unsigned short index;

			if (!m_emptySessionStack->Pop(&index))
			{ // 빈 session 없으면 연결 종료
				closesocket(clientSock);
				continue;
			}


			Session* session = &m_sessionArr[index];
			InterlockedIncrement((LONG*)&session->io_count); // Release 수행 중인 스레드에서 UpdateIO 호출 가능


			// 초기화
			session->session_id = m_sessID++;
			if (m_sessID == 0) m_sessID++;
			*(unsigned*)&session->session_index = index;
			session->sock = clientSock;
			wcscpy_s(session->ip, _countof(session->ip), temp_ip);
			session->port = ntohs(clientAddr.sin_port);
			session->send_post_flag = false;
			//session->send_pend_flag = false;
			session->disconnect = false;
			session->recvPacket = 0;
			session->sendPacket = 0;
			session->last_recv_time = GetTickCount64();
			// Send Q는 release 시에 정리함.
			session->recv_buffer.ClearBuffer();
			session->send_buffer.ClearBuffer();
			session->release_flag = 0;

#ifdef TRACE_SESSION
			session->pending_tracer.trace(enAccept, clientSock);
#endif
			CreateIoCompletionPort((HANDLE)session->sock, m_hcp, (ULONG_PTR)session, 0);



			// 연결 세션 증가
			InterlockedIncrement((LONG*)&m_sessionCnt);

			// 접속 이벤트
			OnClientJoin(*((unsigned long long*) & session->session_id));
			
			
			recvPost(session);

			// RecvPost 내에서 IOCount 증가하므로 현재 스레드에서 증가된 Count 정리
			updateIOCount(session);

		}
		else // Connection Requeset false 반환
		{
			closesocket(clientSock);
		}
	}
}


void CNetServer::runIoThread()
{
	int ret_gqcp;
	DWORD thread_id = GetCurrentThreadId();
#ifdef MONITOR
	LARGE_INTEGER recv_start, recv_end;
	LARGE_INTEGER send_start, send_end;
	LARGE_INTEGER on_recv_st, on_recv_ed;
#endif
	DWORD error_code;

	int cnt = 0;

	while (1)
	{
		DWORD cbTransferred;
		WSAOVERLAPPED* overlapped;
		Session* session;
		ret_gqcp = GetQueuedCompletionStatus(m_hcp, &cbTransferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE); // overlapped媛� null?몄? ?뺤씤 ?곗꽑

		OnWorkerThreadBegin();
		if (overlapped == NULL) // deque 실패 1. timeout 2. 잘못된 소켓 요청(Invalid handle) 3. 일부러 0 0 0  PostQueue)
		{
			break;
		}

		if (ret_gqcp == 0) // 요청 실패 로그만 남긴다 나머지는 뒤에서 처리
		{
			error_code = GetLastError();
			switch (error_code)
			{
			case ERROR_NETNAME_DELETED:
				break;
			case ERROR_OPERATION_ABORTED:
				break;
			default:
				if (error_code == 121L)
				{
					cnt = InterlockedIncrement(&m_semiTimeOut);
				}
				else if (error_code == 1236L)
				{
					cnt = InterlockedIncrement(&m_AbortedByLocal);
				}
				else
				{
					cnt = 0;
				}
				Log(L"SYS", enLOG_LEVEL_ERROR, L"GQCS return 0 [%d] session id : %lld Count %d", error_code, session->GetSessionID(), cnt);

				//OnError(error_code, L"GQCS return 0");
				break;
			}
		}
		if (session == nullptr) // session과 무관한 비동기 요청
		{
			switch (reinterpret_cast<unsigned short>(overlapped))
			{
			case static_cast<unsigned short>(ePost::LOG_PEND):
			{
				writeLog();
				break;
			}
			default:
			{
				Log(L"SYS", enLOG_LEVEL_ERROR, L"fault Post Queue %ld", overlapped);
				break;
			}
			}

			continue;
		}


		if (cbTransferred == 0 || session->disconnect) // IO 실패 혹은 PostQueue 비동기 요청
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enPost, (unsigned long long)overlapped, cbTransferred, session->GetSessionID());
#endif

			switch (reinterpret_cast<unsigned short>(overlapped))
			{
			case static_cast<unsigned short>(ePost::SEND_PEND): // Send 비동기 요청 처리
			{
				trySend(session);
				break;
			}
			case static_cast<unsigned short>(ePost::RELEASE_PEND): // Release 비동기 요청 처리
			{
				release(session);
				break;
			}
			case static_cast<unsigned short>(ePost::CANCEL_IO): // Disconnect 처리 후 IO 걸린거 취소 한번 더
			{
				CancelIoEx((HANDLE)session->sock, NULL);
				break;
			}
			case static_cast<unsigned short>(ePost::CUSTOM_EVENT_PEND): // DB 저장 등.. session release 전에 처리되야 하는 작업들
			{
				/* Todo: Session 내 Queue로 식별자 전달받기. 
				   가상함수 구현으로 처리할 내용 사용자에게 맡기기. */

				break;
			}
			default: // IO 실패인 경우 연결 종료 처리
			{
				disconnect(session); 
				break;
			}
			}

		}
		else {
			if (&session->recv_overlapped == overlapped) // recv 결과 처리
			{
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enRecvResult, cbTransferred, session->GetSessionID());
#endif
				InterlockedAdd(&m_recvByte, cbTransferred); // 모니터링 정보

				session->recv_buffer.MoveRear(cbTransferred);
				session->last_recv_time = GetTickCount64(); // recv 시간 갱신

				while (true)
				{
					NetPacketHeader header;
					if (session->recv_buffer.Peek((char*)&header, sizeof(header)) != sizeof(header))
						break;

					if (header.len == 0 || header.len > session->recv_buffer.GetBufferSize())
					{
						Log(L"SYS", enLOG_LEVEL_DEBUG, L"Packet Error header indicates length [%d]", header.len);
						disconnect(session);
						break;
					}

					int q_size = session->recv_buffer.GetFillSize();
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
					int ret_deq = session->recv_buffer.Dequeue(packet->GetBufferPtrNet(), header.len + sizeof(header));
					packet->MoveWritePos(header.len);
					if (!packet->Decode())
					{
						//CrashDump::Crash();
						Log(L"SYS", enLOG_LEVEL_ERROR, L"Packet Error Decode failed.");
						disconnect(session);
					}
					else
					{
						session->recvPacket++;
						OnRecv(session->GetSessionID(), packet);
					
					}
					CPacket::Free(packet);
#endif

				}

			
				InterlockedAdd(&m_recvPacket, session->recvPacket);
				session->recvPacket = 0;

				bool ret_recv = recvPost(session);

			}
			else if (&session->send_overlapped == overlapped) // send 결과 처리
			{
				InterlockedAdd(&m_sendByte, cbTransferred);
				
				InterlockedAdd(&m_sendPacket, session->sendPacket);
				session->sendPacket = 0;

		

#ifdef TRACE_SESSION
				session->pending_tracer.trace(enSendResult, cbTransferred, session->GetSessionID());
#endif
				OnSend(*(unsigned long long*) & session->session_id, cbTransferred);
				

				session->send_buffer.MoveFront(cbTransferred);


#ifdef AUTO_PACKET				
				while (packet_cnt > 0)
				{
					session->temp_packet[--packet_cnt].~PacketPtr();
				}


#else
				/*
				int packet_cnt = session->send_packet_cnt;
				if (packet_cnt == 0)
				{
					CrashDump::Crash();
				}

				
				while (packet_cnt > 0)
				{

					session->temp_packet[--packet_cnt]->SubRef();
				}*/


#endif
			
				trySend(session);

			}
			else // fault ovelappedIO
			{
				CrashDump::Crash();
			}
		}

		updateIOCount(session);
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


	Session* session = &session_arr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
	if (session->release_flag == 0)
	{
		if (session->session_id == id)
		{
			session->send_q.Enqueue(packet);  // 64 bit 湲곗? 8byte

			SendPost(session);

			ret = true;
		}
		else
		{
			log_arr[5]++;
		}
	}
	UpdateIOCount(session);

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


	Session* session = &m_sessionArr[idx];

	InterlockedIncrement((LONG*)&session->io_count);

	if (session->release_flag == 0 && !session->disconnect)
	{
		if (session->session_id == id)
		{
			packet->AddRef();

			if (session->send_q.Enqueue(packet))
			{
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enSendPacket, 0, session->GetSessionID());
#endif

				sendPend(session);

				ret = true;
			}
			else
			{
				packet->SubRef();
				Log(L"SYS", enLOG_LEVEL_ERROR, L"Full Send Q %lld", session->GetSessionID());
				disconnect(session);
			}
		}

	}
	updateIOCount(session);


	return ret;
}
#endif

void CNetServer::sendPend(Session* session)
{
	if (session->disconnect)
		return;

	if (InterlockedExchange((LONG*)&session->send_post_flag, true) == false)
	{
		InterlockedIncrement((LONG*)&session->io_count);
		PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)session, (LPOVERLAPPED)(ePost::SEND_PEND));
	}

	return;
}

void CNetServer::trySend(Session* session)
{
	session->send_post_flag = false;

	if (session->send_q.GetSize() > 0)
	{
		sendPost(session);
		
	}

}

void CNetServer::DisconnectSession(unsigned long long session_id)
{
	unsigned short idx = session_id >> INDEX_BIT_SHIFT;
	unsigned int id = session_id & ID_MASK;

	if (idx >= m_maxSession)
		CrashDump::Crash();


	Session* session = &m_sessionArr[idx];

	InterlockedIncrement((LONG*)&session->io_count);
	if (session->release_flag == 0)
	{
		if (session->session_id == id)
		{
			disconnect(session);
		}
	}
	updateIOCount(session);

	return;
}



void CNetServer::Log(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szStringFormat, ...)
{
	CLog* log = CLog::Alloc();
	bool ret = false;

	va_list va;
	va_start(va, szStringFormat);
	ret = log->Write(szType, logLevel, szStringFormat, va);
	va_end(va);

	if (ret)
	{
		m_logQueue.Enqueue(log);
		PostQueuedCompletionStatus(m_hcp, 0, 0, reinterpret_cast<LPOVERLAPPED>(ePost::LOG_PEND));
	}

	return;
}

void CNetServer::LogHex(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szLog, BYTE* pByte, int iByteLen)
{
	CLog* log = CLog::Alloc();
	bool ret = false;
	ret = log->WriteHex(szType, logLevel, szLog, pByte, iByteLen);
	if (ret)
	{
		m_logQueue.Enqueue(log);
		PostQueuedCompletionStatus(m_hcp, 0, 0, reinterpret_cast<LPOVERLAPPED>(ePost::LOG_PEND));
	}


	return;
}
void CNetServer::disconnect(Session* session)
{

	if (session->disconnect == 0)
	{
		if (InterlockedExchange((LONG*)&session->disconnect, 1) == 0)
		{
			CancelIoEx((HANDLE)session->sock, NULL);
			InterlockedIncrement((LONG*)&session->io_count);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)session, (LPOVERLAPPED)ePost::CANCEL_IO);
		}
	}
	return;
}

bool CNetServer::recvPost(Session* session)
{
	DWORD recvbytes, flags = 0;
	bool ret = false;

	if (session->disconnect == 0)
	{
		
		ZeroMemory(&session->recv_overlapped, sizeof(session->recv_overlapped));

		int emptySize = session->recv_buffer.GetEmptySize();
		int size1 = session->recv_buffer.DirectEnqueSize();

		WSABUF wsabuf[2];
		int cnt = 1;
		wsabuf[0].buf = session->recv_buffer.GetRearPtr();
		wsabuf[0].len = size1;

		if (size1 < emptySize)
		{
			++cnt;
			wsabuf[1].buf = session->recv_buffer.GetBufPtr();
			wsabuf[1].len = emptySize - size1;
		}

		SOCKET socket = session->sock;
		DWORD error_code;

		InterlockedIncrement((LONG*)&session->io_count);
		int retval = WSARecv(socket, wsabuf, cnt, NULL, &flags, &session->recv_overlapped, NULL);

		if (retval == SOCKET_ERROR)
		{
			if ((error_code = WSAGetLastError()) != ERROR_IO_PENDING)
			{ // 요청 실패
				if (error_code != WSAECONNRESET)
					Log(L"SYS", enLOG_LEVEL_ERROR, L"Recv Failed [%d] session : %lld", error_code, session->GetSessionID());

				int io_temp = updateIOCount(session);

#ifdef TRACE_SESSION
				session->pending_tracer.trace(enRecvFailed, error_code, socket, session->GetSessionID());
#endif
				disconnect(session);

			}
			else
			{
				//tracer.trace(73, session, socket);
				// Pending
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enRecvAsync, error_code, socket, session->GetSessionID());
#endif
				ret = true;
			}
		}
		else
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enRecvSync, 0, socket, session->GetSessionID());
#endif
			//tracer.trace(73, session, socket);
			// 동기IO 동작
#ifdef SYNC_IO_MONITOR
			InterlockedIncrement(&m_syncSend);
#endif
			ret = true;
		}
	}
	

	return ret;
}

void CNetServer::sendPost(Session* session)
{
	LARGE_INTEGER start, end;

	if (session->disconnect == 0)
	{
		if (session->send_post_flag == 0 && (InterlockedExchange((LONG*)&session->send_post_flag, true)) == false)
		{
			int buf_cnt = session->send_q.GetSize();
			if (buf_cnt <= 0)
			{
				session->send_post_flag = false;
				sendPend(session);
			}
			else
			{
				int retval;

				ZeroMemory(&session->send_overlapped, sizeof(session->send_overlapped));




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

				/*
				for (int cnt = 0; cnt < buf_cnt;)
				{
					CPacket* packet;
					if (!session->send_q.Dequeue(&packet)) break;

					wsabuf[cnt].buf = packet->GetBufferPtrNet();
					wsabuf[cnt].len = packet->GetDataSizeNet();
					session->temp_packet[cnt] = packet;

					++cnt;
				}*/

				CPacket* packet;
				while(packet = session->send_q.Front())
				{
					if (session->send_buffer.Enqueue(packet->GetBufferPtrNet(), packet->GetDataSizeNet()))
					{
						CPacket* temp = nullptr;
						while (temp == nullptr)
						{
							session->send_q.Dequeue(&temp); // Enqueue 때문에 실패할 수 있음 
							if (packet == temp)
								break;
						}
						packet->SubRef();
						session->sendPacket++;
					}
					else
						break;
				}

				int fillSize = session->send_buffer.GetFillSize();
				int size1 = session->send_buffer.DirectDequeSize();

				WSABUF wsabuf[2];
				ZeroMemory(wsabuf, sizeof(wsabuf));

				// Send Queue에서 뽑은거 전부 보내야함
				int cnt = 1;
				wsabuf[0].buf = session->send_buffer.GetFrontPtr();
				wsabuf[0].len = size1;

				if (size1 < fillSize)
				{
					++cnt;
					wsabuf[1].buf = session->send_buffer.GetBufPtr();
					wsabuf[1].len = fillSize - size1;
				}
#endif

				//session->send_packet_cnt = buf_cnt;
				DWORD sendbytes;

				SOCKET socket = session->sock;

				int temp_io = InterlockedIncrement((LONG*)&session->io_count); // 요청 발생
				retval = WSASend(socket, wsabuf, cnt, nullptr, 0, &session->send_overlapped, nullptr);


				DWORD error_code;
				if (retval == SOCKET_ERROR)
				{
					if ((error_code = WSAGetLastError()) != WSA_IO_PENDING) // 요청 실패
					{
						if(error_code != WSAECONNRESET)
							Log(L"SYS", enLOG_LEVEL_ERROR, L"Send Failed [%d] session : %lld", error_code, session->GetSessionID());
						int io_temp = updateIOCount(session);
#ifdef TRACE_SESSION
						session->pending_tracer.trace(enSendFailed, error_code, socket, session->GetSessionID());
#endif

						disconnect(session);
					}
					else
					{
						//tracer.trace(72, session, socket);
						// Pending
#ifdef TRACE_SESSION
						session->pending_tracer.trace(enSendAsync, error_code, socket, session->GetSessionID());
#endif
					}
				}
				else
				{
					// 동기 입출력 동작
#ifdef TRACE_SESSION
					session->pending_tracer.trace(enSendSync, 0, socket, session->GetSessionID());
#endif

#ifdef SYNC_IO_MONITOR
					InterlockedIncrement(&m_syncSend);
#endif

				}
			}
		}
	}
	return;
}

int CNetServer::updateIOCount(Session* session)
{
	int temp;
	//tracer.trace(76, session, session->session_id);
	if ((temp = InterlockedDecrement((LONG*)&session->io_count)) == 0)
	{
		releasePend(session);
	}
	else if (temp < 0)
	{
		CrashDump::Crash();
	}

	return temp;
}


void CNetServer::releasePend(Session* session)
{
	
	unsigned long long flag = *((unsigned long long*)(&session->io_count));
	if (flag == 0)
	{
		if (InterlockedCompareExchange64((LONG64*)&session->io_count, 0x100000000, flag) == flag)
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enReleasePend, session->sock, session->GetSessionID());
#endif

			// PostQueue 
			InterlockedIncrement((LONG*)&session->io_count);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)session, (LPOVERLAPPED)ePost::RELEASE_PEND);
			
		}
	}

	return;
}

void CNetServer::release(Session* session)
{
	if (session->disconnect != 1)
		CrashDump::Crash();


	OnClientLeave(session->GetSessionID());
	
	unsigned long long id = session->GetSessionID();
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
	/*
	int remain = session->send_packet_cnt;
	while (remain > 0)
	{
		session->temp_packet[--remain]->SubRef();
	}
	*/

#endif

	SOCKET sock = session->sock;

	closesocket(sock);
	session->sock = INVALID_SOCKET;

	m_emptySessionStack->Push(session->session_index);


#ifdef TRACE_SESSION
	session->pending_tracer.trace(enRelease, sock, id);
#endif

	InterlockedDecrement((LONG*)&m_sessionCnt);

	return;
}


void CNetServer::Show()
{

	unsigned long long pre = m_preAccept;
	m_preAccept = m_totalAccept;
	int tps = m_preAccept - pre;
	int recvByte = InterlockedExchange(&m_recvByte, 0);
	int sendByte = InterlockedExchange(&m_sendByte, 0);
	int recvPacket = InterlockedExchange(&m_recvPacket, 0);
	int sendPacket = InterlockedExchange(&m_sendPacket, 0);

	wprintf(L"------------------------------------------------------------\n");
	wprintf(L"Total Accept : %lld | TPS : %d | Accept Error : %d\n", m_totalAccept, tps, m_acceptErr);
	
	wprintf(L"Session: %d\n", m_sessionCnt);
	wprintf(L"PacketPool : Alloc %lld / Use %d\n", CPacket::GetPacketAlloc(), CPacket::GetUsePool());
	
#ifdef SYNC_IO_MONITOR
	wprintf(L"SYNC RECV / SEND : %d / %d\n", m_syncRecv, m_syncSend);
#endif

	wprintf(L"Send KBytes Per Sec : %d KB\n"
		L"Recv KBytes Per Sec : %d KB\n"
		L"SendPacket Per Sec : %d\n"
		L"RecvPacket Per Sec : %d\n",
		sendByte >> 10, recvByte >> 10, sendPacket, recvPacket);


	return;
}


void CNetServer::runTimeoutThread()
{
	int max_session = m_maxSession;
	while (m_isRunning)
	{
		Sleep(1000);

		for (int i = 0; i < max_session; i++)
		{
			Session* session = &m_sessionArr[i];
			ULONG64 now_tick = GetTickCount64();
			InterlockedIncrement((LONG*)&m_sessionArr[i].io_count);
			if (!session->release_flag && !session->disconnect)
			{
				ULONG64 last_tick = session->last_recv_time;
				if ((long long)(now_tick - last_tick) >= 40000) {
					unsigned long long id = session->GetSessionID();
					OnClientTimeout(id);
					Log(L"SYS", enLOG_LEVEL_DEBUG, L"Timeout session %lld tick %lld %lld", id, now_tick, last_tick);
				}
			}
			updateIOCount(session);
		}
	}

	return;
}


void CNetServer::writeLog()
{
	CLog* log = nullptr;

	while (log == nullptr)
	{
		m_logQueue.Dequeue(&log); // Enqueue 때문에 실패할 수 있음 

	}

	log->WriteFile();
	CLog::Free(log);
}

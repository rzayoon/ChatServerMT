#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")


#include <stdio.h>
#include <stdlib.h>

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>


#include "CrashDump.h"

#include "LockFreeQueue.h"
#include "LockFreeStack.h"

#include "MemoryPoolTls.h"
#include "CPacket.h"

#include "Tracer.h"
#include "RingBuffer.h"
#include "session.h"
#include "CNetServer.h"
#include "NetProtocol.h"
#include "CLog.h"


alignas(64) LONG SEMTIMEOUT = 0;
alignas(64) LONG ABORTEDBYLOCAL = 0;


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

	m_isRunning = true;



	// IOCP »ý¼º
	m_hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_iocpActiveNum);
	if (m_hcp == NULL)
	{
		CrashDump::Crash();
	}

	// Accept Thread
	m_hAcceptThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AcceptThread, this, CREATE_SUSPENDED, NULL);
	if (m_hAcceptThread == NULL)
	{
		CrashDump::Crash();
	}
	
	// Time out thread
	m_hTimeOutThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TimeoutThread, this, 0, NULL);
	if (m_hTimeOutThread == NULL)
	{
		CrashDump::Crash();
	}


	// Worker Thread »ý¼º
	m_hWorkerThread = new HANDLE[m_iocpWorkerNum];
	for (int i = 0; i < m_iocpWorkerNum; i++)
	{
		m_hWorkerThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IoThread, this, CREATE_SUSPENDED, NULL);
		if (m_hWorkerThread[i] == NULL)
		{
			CrashDump::Crash();
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

	
	// blocking Accept Á¾·á Ã³¸® À¯µµ.
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

	// session Á¾·á ¿äÃ»
	for (int i = 0; i < m_maxSession; i++)
	{
		if (!m_sessionArr[i].release_flag)
		{
			DisconnectSession(*(unsigned long long*) & m_sessionArr[i].session_id);
		}
	}
	
	// session release ´ë±â
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

	// worker Á¾·á À¯µµ
	// PostQueuedCompletionStatus 0 0 0
	for (int i = 0; i < m_iocpWorkerNum - 2; i++)
		PostQueuedCompletionStatus(m_hcp, 0, 0, 0);

	WaitForMultipleObjects(m_iocpWorkerNum, hExit, TRUE, INFINITE);

	delete[] hExit;
	delete[] m_sessionArr;

	CloseHandle(m_hcp);

	


	return;
}





unsigned long _stdcall CNetServer::AcceptThread(void* param)
{
	CNetServer* server = (CNetServer*)param;
	wprintf(L"%d Accept thread On...\n", GetCurrentThreadId());
	server->RunAcceptThread();
	// this call°ú Æ÷ÀÎÅÍ call Â÷ÀÌ ¾øÀ½ ÄÚµù ÆíÀÇ»ó ¸â¹öÇÔ¼ö È£Ãâ
	// mov reg1 [this]/[server]
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

unsigned long _stdcall CNetServer::TimeoutThread(void* param)
{
	CNetServer* server = (CNetServer*)param;

	server->RunTimeoutThread();

	return 0;

}



void CNetServer::RunAcceptThread()
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

	// listen ¼ÒÄÏÀÇ ¼Û½Å ¹öÆÛ Å©±â°¡ acceptµÈ ¼ÒÄÏ¿¡ µ¿ÀÏÇÏ°Ô ¼³Á¤µÈ´Ù.
#ifdef SEND_ZEROCOPY
	int sendBufSize = 0;
	setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
#endif

	// nagle ¾Ë°í¸®Áò ÆÄ¼­¿¡¼­ Àû¿ë
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

	// ºí·ÎÅ· Accept µ¿ÀÛ
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

		// È­ÀÌÆ®¸®½ºÆ® OR ºí·¢¸®½ºÆ® ¼³Á¤ °¡´É
		if (OnConnectionRequest(temp_ip, temp_port))
		{
			unsigned short index;
#ifdef STACK_INDEX
			if (!empty_session_stack.Pop(&index))
			{ // ºó session ¾øÀ¸¸é ¿¬°á Á¾·á
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
			InterlockedIncrement((LONG*)&session->io_count); // Release ¼öÇà ÁßÀÎ ½º·¹µå¿¡¼­ UpdateIO È£Ãâ °¡´É


			// ÃÊ±âÈ­
			session->session_id = m_sess_id++;
			if (m_sess_id == 0) m_sess_id++;
			*(unsigned*)&session->session_index = index;
			session->sock = clientSock;
			wcscpy_s(session->ip, _countof(session->ip), temp_ip);
			session->port = ntohs(clientAddr.sin_port);
			session->send_post_flag = false;
			session->send_pend_flag = false;
			session->send_packet_cnt = 0;
			session->disconnect = false;
			session->last_recv_time = GetTickCount64();
			// Send Q´Â release ½Ã¿¡ Á¤¸®ÇÔ.
			session->recv_buffer.ClearBuffer();
			session->send_buffer.ClearBuffer();
			session->release_flag = 0;

#ifdef TRACE_SESSION
			session->pending_tracer.trace(enAccept, clientSock);
#endif
			CreateIoCompletionPort((HANDLE)session->sock, m_hcp, (ULONG_PTR)session, 0);



			// ¿¬°á ¼¼¼Ç Áõ°¡
			InterlockedIncrement((LONG*)&m_sessionCnt);

			// Á¢¼Ó ÀÌº¥Æ®
			OnClientJoin(*((unsigned long long*) & session->session_id));
			
			
			RecvPost(session);

			// RecvPost ³»¿¡¼­ IOCount Áõ°¡ÇÏ¹Ç·Î ÇöÀç ½º·¹µå¿¡¼­ Áõ°¡µÈ Count Á¤¸®
			UpdateIOCount(session);

		}
		else // Connection Requeset false ¹ÝÈ¯
		{
			closesocket(clientSock);
		}
	}
}


void CNetServer::RunIoThread()
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
		ret_gqcp = GetQueuedCompletionStatus(m_hcp, &cbTransferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE); // overlappedê°€ null?¸ì? ?•ì¸ ?°ì„ 

		OnWorkerThreadBegin();
		if (overlapped == NULL) // deque ½ÇÆÐ 1. timeout 2. Àß¸øµÈ ¼ÒÄÏ ¿äÃ»(Invalid handle) 3. ÀÏºÎ·¯ 0 0 0 queueing PostQueue)
		{
			break;
		}

		if (ret_gqcp == 0)
		{
			// µð¹ö±ë¿ë
			error_code = GetLastError();
			switch (error_code)
			{
			case ERROR_NETNAME_DELETED:
				break;
			case ERROR_OPERATION_ABORTED:
				break;
			default:
#ifdef TRACE_SERVER
				tracer.trace(00, session, error_code);
#endif
				if (error_code == 121L)
				{
					cnt = InterlockedIncrement(&SEMTIMEOUT);
				}
				else if (error_code == 1236L)
				{
					cnt = InterlockedIncrement(&ABORTEDBYLOCAL);
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



		if (cbTransferred == 0 || session->disconnect)
		{
#ifdef TRACE_SERVER
			tracer.trace(78, session, session->session_id);
#endif
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enPost, (unsigned long long)overlapped, cbTransferred, session->GetSessionID());
#endif

			switch ((unsigned long long)overlapped)
			{
			case enSEND_PEND:
			{
				session->send_post_flag = false;

				if (session->send_q.GetSize() > 0)
				{
					SendPost(session);
					SendPend(session);
				}

				break;
			}
			case enRELEASE_PEND:
			{
				Release(session);
				break;
			}
			case enCANCEL_IO:
			{
				
				break;
			}
			default:
			{
				Disconnect(session);
				break;
			}
			}

		}
		else {
			if (&session->recv_overlapped == overlapped) // recv
			{
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enRecvResult, cbTransferred, session->GetSessionID());
#endif

				session->last_recv_time = GetTickCount64();

				InterlockedAdd(&m_recvByte, cbTransferred);
				session->recv_buffer.MoveRear(cbTransferred);

				while (true)
				{
					NetPacketHeader header;

					if (session->recv_buffer.Peek((char*)&header, sizeof(header)) != sizeof(header))
						break;

					int q_size = session->recv_buffer.GetFillSize();
					if (header.len + sizeof(header) > q_size)
						break;

					if (header.len == 0 || header.len > session->recv_buffer.GetEmptySize()) {
						Log(L"SYS", enLOG_LEVEL_DEBUG, L"Header Length Error %d", header.len);
						Disconnect(session);

						break;
					}

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
						Log(L"SYS", enLOG_LEVEL_ERROR, L"Packet Error");
						Disconnect(session);
					}
					else
					{
						OnRecv(session->GetSessionID(), packet);
					
					}
					CPacket::Free(packet);
#endif

				}


				bool ret_recv = RecvPost(session);

			}
			else if (&session->send_overlapped == overlapped) // send °á°ú Ã³¸®
			{
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enSendResult, cbTransferred, session->GetSessionID());
#endif
				OnSend(*(unsigned long long*) & session->session_id, cbTransferred);
				if (session->send_sock != session->sock)
					CrashDump::Crash();

				InterlockedAdd(&m_sendByte, cbTransferred);
				
				unsigned long long local_maxTransferred = m_maxTransferred;
				if (local_maxTransferred < cbTransferred)
					InterlockedCompareExchange(&m_maxTransferred, cbTransferred, local_maxTransferred);



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
				session->send_buffer.MoveFront(cbTransferred);


#endif

				session->send_post_flag = false;

				//tracer.trace(32, session, session->session_id);
				if (session->send_q.GetSize() > 0)
				{
					SendPost(session);
					SendPend(session);
				}
			}
			else // fault ovelappedIO
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
			session->send_q.Enqueue(packet);  // 64 bit ê¸°ì? 8byte

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

				SendPend(session);

				ret = true;
			}
			else
			{
				packet->SubRef();
				Log(L"SYS", enLOG_LEVEL_ERROR, L"Full Send Q %lld", session->GetSessionID());
				Disconnect(session);
			}
		}

	}
	UpdateIOCount(session);


	return ret;
}
#endif

void CNetServer::SendPend(Session* session)
{
	if (session->disconnect)
		return;

	if (InterlockedExchange((LONG*)&session->send_post_flag, true) == false)
	{
		InterlockedIncrement((LONG*)&session->io_count);
		PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)session, (LPOVERLAPPED)enSEND_PEND);
	}

	return;
}

void CNetServer::DisconnectSession(unsigned long long session_id)
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

void CNetServer::Disconnect(Session* session)
{

	if (session->disconnect == 0)
	{
		if (InterlockedCompareExchange((LONG*)&session->disconnect, 1, 0) == 0) {
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enDisconnect, session->sock, session->GetSessionID());
#endif
			
			CancelIOSession(session);
		}
	}

	return;
}

bool CNetServer::RecvPost(Session* session)
{
	DWORD recvbytes, flags = 0;
	bool ret = false;

	int temp_pend = InterlockedIncrement((LONG*)&session->pend_count);
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
		session->recv_sock = socket;
		DWORD error_code;

		InterlockedIncrement((LONG*)&session->io_count);
		int retval = WSARecv(socket, wsabuf, cnt, NULL, &flags, &session->recv_overlapped, NULL);

		if (retval == SOCKET_ERROR)
		{
			if ((error_code = WSAGetLastError()) != ERROR_IO_PENDING)
			{ // ¿äÃ» ½ÇÆÐ
				if (error_code != WSAECONNRESET)
					Log(L"SYS", enLOG_LEVEL_ERROR, L"Recv Failed [%d] session : %lld", error_code, session->GetSessionID());

				int io_temp = UpdateIOCount(session);
#ifdef TRACE_SERVER
				tracer.trace(1, session, error_code, socket);
#endif
#ifdef TRACE_SESSION
				session->pending_tracer.trace(enRecvFailed, error_code, socket, session->GetSessionID());
#endif
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
			// µ¿±âIO µ¿ÀÛ
#ifdef SYNC_IO_MONITOR
			InterlockedIncrement(&m_syncSend);
#endif
			ret = true;
		}
	}
	UpdatePendCount(session);

	return ret;
}

void CNetServer::SendPost(Session* session)
{
	LARGE_INTEGER start, end;

	int temp_pend = InterlockedIncrement((LONG*)&session->pend_count);
	if (session->disconnect == 0)
	{

		if ((InterlockedExchange((LONG*)&session->send_post_flag, true)) == false)
		{
			int buf_cnt = session->send_q.GetSize();
			if (buf_cnt <= 0)
			{
				session->send_post_flag = false;
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
							session->send_q.Dequeue(&temp); // Enqueue ¶§¹®¿¡ ½ÇÆÐÇÒ ¼ö ÀÖÀ½
							if (packet == temp)
								break;
						}
						packet->SubRef();
					}
					else
						break;
				}

				int fillSize = session->send_buffer.GetFillSize();
				int size1 = session->send_buffer.DirectDequeSize();

				WSABUF wsabuf[2];
				ZeroMemory(wsabuf, sizeof(wsabuf));

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
				session->send_sock = socket;

				int temp_io = InterlockedIncrement((LONG*)&session->io_count); // ¿äÃ» ¹ß»ý
				retval = WSASend(socket, wsabuf, cnt, NULL, 0, &session->send_overlapped, NULL);


				DWORD error_code;
				if (retval == SOCKET_ERROR)
				{
					if ((error_code = WSAGetLastError()) != WSA_IO_PENDING) // ¿äÃ» ½ÇÆÐ
					{
						if(error_code != WSAECONNRESET)
							Log(L"SYS", enLOG_LEVEL_ERROR, L"Send Failed [%d] session : %lld", error_code, session->GetSessionID());
						int io_temp = UpdateIOCount(session);

#ifdef TRACE_SERVER
						tracer.trace(2, session, error_code, socket);
#endif
#ifdef TRACE_SESSION
						session->pending_tracer.trace(enSendFailed, error_code, socket, session->GetSessionID());
#endif
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
					// µ¿±â ÀÔÃâ·Â µ¿ÀÛ
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
	UpdatePendCount(session);


	return;
}

int CNetServer::UpdateIOCount(Session* session)
{
	int temp;
	//tracer.trace(76, session, session->session_id);
	if ((temp = InterlockedDecrement((LONG*)&session->io_count)) == 0)
	{
		ReleasePend(session);
	}
	else if (temp < 0)
	{
		CrashDump::Crash();
	}

	return temp;
}

void CNetServer::UpdatePendCount(Session* session)
{
	// disconnect ?œë²ˆ???•ì¸
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
			session->pending_tracer.trace(enCancelIO, session->sock, session->GetSessionID());
#endif
			InterlockedIncrement((LONG*)&session->io_count);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)session, (LPOVERLAPPED)enCANCEL_IO); 
			CancelIoEx((HANDLE)session->sock, NULL);
		}
	}

	return;
}


void CNetServer::ReleasePend(Session* session)
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
			session->pending_tracer.trace(enReleasePend, session->sock, session->GetSessionID());
#endif

			// PostQueue 
			InterlockedIncrement((LONG*)&session->io_count);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)session, (LPOVERLAPPED)enRELEASE_PEND);
			
		}
	}

	return;
}

void CNetServer::Release(Session* session)
{
	
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



#ifdef STACK_INDEX
	empty_session_stack.Push(session->session_index);
#else
	session->used = false;
#endif

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


	wprintf(L"-----------------------------------------\n");
	wprintf(L"Total Accept : %lld | TPS : %d | Accept Error : %d\n", m_totalAccept, tps, m_acceptErr);
	
	wprintf(L"Session: %d\n", m_sessionCnt);
	wprintf(L"PacketPool Alloc / Use : %lld / %d\n", CPacket::GetPacketAlloc(), CPacket::GetUsePool());
	
#ifdef SYNC_IO_MONITOR
	wprintf(L"SYNC RECV / SEND : %d / %d\n", m_syncRecv, m_syncSend);
#endif

	wprintf(L"Send BPS : %d\n"
		L"Recv BPS : %d\n"
		L"Max Send bytes per once : %lld\n",
		sendByte, recvByte, m_maxTransferred);


	return;
}


void CNetServer::RunTimeoutThread()
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
					session->last_recv_time = now_tick;
					Log(L"SYS", enLOG_LEVEL_DEBUG, L"Timeout session %lld tick %lld %lld", id, now_tick, last_tick);
				}
			}
			UpdateIOCount(session);
		}
	}

	return;
}
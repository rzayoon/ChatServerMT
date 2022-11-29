#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdio.h>

#include "LockFreeQueue.h"
#include "CLanClient.h"
#include "NetProtocol.h"
#include "CrashDump.h"
#include "CLog.h"



bool CLanClient::Connect(const wchar_t* serverIp, unsigned short port,
	int iocpWorker, int iocpActive, bool nagle)
{
	bool ret = false;
	


	if (m_isConnected)
	{
		OnError(90, L"Connected already\n");
		return ret;
	}

	m_nagle = nagle;

	wcscpy_s(m_serverIp, serverIp);
	m_serverPort = port;

	m_iocpActiveNum = iocpActive;
	m_iocpWorkerNum = iocpWorker;

	// WinSock 초기화
	if (!m_isInit)
	{
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		{
			Log(L"ERR", enLOG_LEVEL_ERROR, L"WSAStartup failed");
			CrashDump::Crash();
		}


		// IOCP 생성
		m_hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_iocpActiveNum);
		if (m_hcp == NULL)
		{
			Log(L"ERR", enLOG_LEVEL_ERROR, L"CreateIOCP failed");
			CrashDump::Crash();
		}

		// Worker Thread 생성
		m_hWorkerThread = new HANDLE[m_iocpWorkerNum];
		if (m_hWorkerThread == nullptr)
		{
			Log(L"ERR", enLOG_LEVEL_ERROR, L"mem alloc failed");
			CrashDump::Crash();
		}

		for (int i = 0; i < m_iocpWorkerNum; i++)
		{
			m_hWorkerThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IoThread, this, CREATE_SUSPENDED, NULL);
			if (m_hWorkerThread[i] == NULL)
			{
				Log(L"ERR", enLOG_LEVEL_ERROR, L"Create Thread");
				CrashDump::Crash();
			}
		}



		for (int i = 0; i < m_iocpWorkerNum; i++)
		{
			ResumeThread(m_hWorkerThread[i]);
		}
		m_isInit = true;
	}

	m_serverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_serverSock == INVALID_SOCKET)
	{
		OnError(WSAGetLastError(), L"socket failed");
		ret = false;
	}
	else
	{
		

		//rst
		LINGER linger;
		linger.l_linger = 0;
		linger.l_onoff = true;
		setsockopt(m_serverSock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));
		
		//nagle
		setsockopt(m_serverSock, IPPROTO_TCP, TCP_NODELAY, (char*)&m_nagle, sizeof(m_nagle));

#ifdef SEND_ZEROCOPY
		int sendBufSize = 0;
		setsockopt(m_serverSock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
#endif


		ret = ConnectSession();
		m_isConnected = true;
	}


	if (!ret)
	{
		// 정리
		wprintf(L"connect failed\n");
		ReleaseClient();

	}

	return ret;
}

void CLanClient::Disconnect()
{
	if (!m_isConnected)
	{
		OnError(98, L"Not Running Yet\n");
		return;
	}

	m_isConnected = false;

	// session 연결 끊기
	DisconnectSession();
	
	// session 릴리즈될 때까지 대기
	while (m_releaseFlag == 0)
	{
		Sleep(0); 
	}

	ReleaseClient();

	wprintf(L"Disconnected\n");

	return;
}



void CLanClient::ReleaseClient(void)
{

	m_isConnected = false;

	return;
}

unsigned long _stdcall CLanClient::IoThread(void* param)
{
	CLanClient* client = (CLanClient*)param;
	wprintf(L"%d worker thread On...\n", GetCurrentThreadId());
	client->RunIoThread();
	wprintf(L"%d IO thread end\n", GetCurrentThreadId());
	return 0;
}

inline void CLanClient::RunIoThread()
{
	int ret_gqcp;
	DWORD thread_id = GetCurrentThreadId();
	DWORD error_code;
	ULONG* key;

	int cnt = 0;

	while (1)
	{
		DWORD cbTransferred;
		WSAOVERLAPPED* overlapped;
		ret_gqcp = GetQueuedCompletionStatus(m_hcp, &cbTransferred, (PULONG_PTR)&key, (LPOVERLAPPED*)&overlapped, INFINITE); // overlapped가 null인지 확인 우선

		OnWorkerThreadBegin();
		if (overlapped == NULL) // deque 실패 1. timeout 2. 잘못 호출(Invalid handle) 3. 임의로 queueing 한 것(PostQueue)
		{
			if (key == nullptr)
			{
				break;
			}
		}

		if (ret_gqcp == 0)
		{
			//에러코드 로깅
			error_code = GetLastError();
			switch (error_code)
			{
			case ERROR_NETNAME_DELETED:
				break;
			case ERROR_OPERATION_ABORTED:
				break;
			default:


				Log(L"SYS", enLOG_LEVEL_ERROR, L"GQCS return 0 [%d] session id : %lld Count %d", error_code, m_sessionId, cnt);
			

				//OnError(error_code, L"GQCS return 0");
				break;

			}

		}



		if (cbTransferred == 0 || m_disconnect) // Pending 후 I/O 처리 실패
		{
#
			if ((unsigned long long)overlapped == 1)
			{
				m_sendFlag = false;

				SendPost();

			}
			else if (&m_recvOverlapped == overlapped && !m_leaveFlag)
			{
				Leave();
			}

		}
		else {
			if (&m_recvOverlapped == overlapped) // recv 결과 처리
			{
				m_recvQ.MoveRear(cbTransferred);
	
				while (true)
				{
					LanPacketHeader header;

					if (m_recvQ.Peek((char*)&header, sizeof(header)) != sizeof(header))
						break;

					int q_size = m_recvQ.GetFillSize();
					if (header.len + sizeof(header) > q_size)
						break;

					if (header.len > m_recvQ.GetEmptySize())
						DisconnectSession();


					CPacket* packet = CPacket::Alloc();
					int ret_deq = m_recvQ.Dequeue(packet->GetBufferPtrLan(), header.len + sizeof(header));
					packet->MoveWritePos(header.len);
				


					OnRecv(packet);

					CPacket::Free(packet);
				}
				bool ret_recv = RecvPost();

			}
			else if (&m_sendOverlapped == overlapped) // send 결과 처리
			{

				int packet_cnt = m_sentPacketCnt;
				if (packet_cnt == 0)
				{
					CrashDump::Crash();
				}

				while (packet_cnt > 0)
				{

					m_sentPacket[--packet_cnt]->SubRef();
				}

				m_sentPacketCnt = 0;
				m_sendFlag = false;


				//tracer.trace(32, session, session->session_id);
				if (m_sendQ.GetSize() > 0)
					SendPost();

			}
			else // send, recv 다 아님(다른 session의 overlapped 전달)
			{
				CrashDump::Crash();
			}
		}

		UpdateIOCount();
		OnWorkerThreadEnd();
	}

	return;
}

bool CLanClient::SendPacket(CPacket* packet)
{
	bool ret = false;

	int id = m_sessionId;

	InterlockedIncrement((LONG*)&m_ioCount);
#ifdef TRACE_SESSION
	session->pending_tracer.trace(enSendPacket, 0, GetTickCount64());
#endif
	if (m_releaseFlag == 0 && !m_disconnect)
	{
		if (m_sessionId == id)
		{
			packet->AddRef();

			if (m_sendQ.Enqueue(packet))
			{
				if (InterlockedExchange((LONG*)&m_sendFlag, true) == false)
				{
					InterlockedIncrement((LONG*)&m_ioCount);
					PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)&m_serverSock, (LPOVERLAPPED)1);
				}
				ret = true;
			}
			else
			{
				packet->SubRef();

				Log(L"SYS", enLOG_LEVEL_ERROR, L"Full Send Q");
			}
		}

	}
	UpdateIOCount();




	return ret;
}


bool CLanClient::ConnectSession()
{
	bool ret;

	SOCKADDR_IN addr;
	ZeroMemory(&addr, sizeof(addr));

	addr.sin_family = AF_INET;

	InetPtonW(AF_INET, m_serverIp, &addr.sin_addr.s_addr);
	addr.sin_port = htons(m_serverPort);

	int ret_con = connect(m_serverSock, (SOCKADDR*)&addr, sizeof(addr));
	if (ret_con == SOCKET_ERROR)
	{
		OnError(WSAGetLastError(), L"connect failed");
		closesocket(m_serverSock);
		ret = false;
	}
	else
	{

		InterlockedIncrement((LONG*)&m_ioCount);

		ret = true;
		static unsigned int id_generator;

		m_sessionId = id_generator++;

		m_sendFlag = false;
		m_sentPacketCnt = 0;
		m_leaveFlag = false;
		m_disconnect = false;

		m_recvQ.ClearBuffer();

		CreateIoCompletionPort((HANDLE)m_serverSock, m_hcp, (ULONG_PTR)&m_serverSock, 0);

		m_releaseFlag = 0; // 준비 끝


		OnEnterServerJoin();

		RecvPost();

		UpdateIOCount();

	}

	return ret;
}

inline void CLanClient::DisconnectSession()
{
	if (m_disconnect == 0)
	{
		if (InterlockedCompareExchange((LONG*)&m_disconnect, 1, 0) == 0) {
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enDisconnect, session->sock, GetTickCount64());
#endif
			// pending cnt == 0 이면 cancel IO 
			
			CancelIOPend();

		}
	}
	return;
}

inline bool CLanClient::RecvPost()
{
	DWORD recvbytes, flags = 0;
	bool ret = false;
	DWORD error_code;

	int temp_pend = InterlockedIncrement((LONG*)&m_pendCount);
	if (m_disconnect == 0)
	{
		InterlockedIncrement((LONG*)&m_ioCount);
		ZeroMemory(&m_recvOverlapped, sizeof(m_recvOverlapped));

		int emptySize = m_recvQ.GetEmptySize();
		int size1 = m_recvQ.DirectEnqueSize();

		WSABUF wsabuf[2];
		int cnt = 1;
		wsabuf[0].buf = m_recvQ.GetRearPtr();
		wsabuf[0].len = size1;

		if (size1 < emptySize)
		{
			++cnt;
			wsabuf[1].buf = m_recvQ.GetBufPtr();
			wsabuf[1].len = emptySize - size1;
		}

	

		int retval = WSARecv(m_serverSock, wsabuf, cnt, NULL, &flags, &m_recvOverlapped, NULL);

		if (retval == SOCKET_ERROR)
		{
			if ((error_code = WSAGetLastError()) != ERROR_IO_PENDING)
			{ // 요청이 실패
				if (error_code != WSAECONNRESET)
					Log(L"SYS", enLOG_LEVEL_ERROR, L"Recv Failed [%d] session", error_code);
				Leave();
				int io_temp = UpdateIOCount();
			}
			else
			{
				//tracer.trace(73, session, socket);
				// Pending
				ret = true;
			}
		}
		else
		{
			//tracer.trace(73, session, socket);
			//동기 recv
			ret = true;
		}
	}
	UpdatePendCount();

	return ret;
}

inline void CLanClient::SendPost()
{

	int temp_pend = InterlockedIncrement((LONG*)&m_pendCount);
	if (m_disconnect == 0)
	{

		if ((InterlockedExchange((LONG*)&m_sendFlag, true)) == false) // compare exchange
		{
			long long buf_cnt = m_sendQ.GetSize();
			if (buf_cnt <= 0)
			{
				m_sendFlag = false;
			}
			else
			{
				int temp_io = InterlockedIncrement((LONG*)&m_ioCount);
				int retval;

				ZeroMemory(&m_sendOverlapped, sizeof(m_sendOverlapped));

				// 개선 필요
				if (buf_cnt > MAX_WSABUF)
					buf_cnt = MAX_WSABUF;

				WSABUF wsabuf[MAX_WSABUF];
				ZeroMemory(wsabuf, sizeof(wsabuf));


				CPacket* packet;
				for (int cnt = 0; cnt < buf_cnt;)
				{
					if (!m_sendQ.Dequeue(&packet)) continue;

					wsabuf[cnt].buf = packet->GetBufferPtrLan();
					wsabuf[cnt].len = packet->GetDataSizeLan();
					m_sentPacket[cnt] = packet;

					++cnt;
				}

				m_sentPacketCnt = buf_cnt;
				DWORD sendbytes;


				SOCKET socket = m_serverSock;
				
				retval = WSASend(socket, wsabuf, buf_cnt, NULL, 0, &m_sendOverlapped, NULL);

				DWORD error_code;
				if (retval == SOCKET_ERROR)
				{
					if ((error_code = WSAGetLastError()) != WSA_IO_PENDING) // 요청 자체가 실패
					{
						if(error_code != WSAECONNRESET)
							Log(L"SYS", enLOG_LEVEL_ERROR, L"Send Failed [%d] session", error_code);
						int io_temp = UpdateIOCount();

					}
					else
					{
						// Pending

					}
				}
				else
				{
					//동기처리

				}
			}
		}
	}
	UpdatePendCount();


	return;
}

inline int CLanClient::UpdateIOCount()
{
	int temp;
	//tracer.trace(76, session, session->session_id);
	if ((temp = InterlockedDecrement((LONG*)&m_ioCount)) == 0)
	{
		Release();
	}
	else if (temp < 0)
	{
		CrashDump::Crash();
	}

	return temp;
}

inline void CLanClient::UpdatePendCount()
{
	// disconnect 한번에 확인
	int temp;
	if ((temp = InterlockedDecrement((LONG*)&m_pendCount)) == 0)
	{
		CancelIOPend();
	}

	return;

}

void CLanClient::CancelIOPend()
{
	unsigned long long flag = *((unsigned long long*)(&m_pendCount));
	if (flag == 0x100000000)
	{
		if (InterlockedCompareExchange64((LONG64*)&m_pendCount, 0x200000000, flag) == flag)
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enCancelIO, session->sock, GetTickCount64());
#endif
			InterlockedIncrement((LONG*)&m_ioCount);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)&m_serverSock, &m_recvOverlapped);
			CancelIoEx((HANDLE)m_serverSock, NULL);
		}
	}

	return;
}


inline void CLanClient::Release()
{

	unsigned long long flag = *((unsigned long long*)(&m_ioCount));
	if (flag == 0)
	{
		if (InterlockedCompareExchange64((LONG64*)&m_ioCount, 0x100000000, flag) == flag)
		{

			if (!m_leaveFlag) CrashDump::Crash(); // 삭제 처리 확인
			
			m_sessionId = 0;


			CPacket* packet;
			while (m_sendQ.Dequeue(&packet))
			{
				packet->SubRef();
			}

			int remain = m_sentPacketCnt;
			while (remain > 0)
			{
				m_sentPacket[--remain]->SubRef();
			}

			closesocket(m_serverSock);
			m_serverSock = INVALID_SOCKET;

			ReleaseClient();

		}
	}

	return;
}

void CLanClient::Leave()
{

	unsigned int flag = *((unsigned int*)(&m_leaveFlag));
	if (flag == 0x0)
	{
		if (InterlockedCompareExchange((LONG*)&m_leaveFlag, 0x1, flag) == flag)
		{
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enLeave, session->sock, GetTickCount64());
#endif
			OnLeaveServer();
			
		}
	}

	return;
}


void CLanClient::Show()
{
	wprintf(L"-----------------------------------------\n");
	wprintf(L"PacketPool Use: %d\n", CPacket::GetUsePool());


	return;
}

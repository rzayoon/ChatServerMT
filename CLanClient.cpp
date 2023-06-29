#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdio.h>



#include <Windows.h>



#include "CrashDump.h"

#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "MemoryPoolTls.h"

#include "RingBuffer.h"
#include "CPacket.h"
#include "Tracer.h"


#include "CLanClient.h"
#include "NetProtocol.h"
#include "CLog.h"



CLanClient::CLanClient()
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

CLanClient::~CLanClient()
{

}

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

	if (!m_isInit)
	{
		// WinSock 초기화
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
			m_hWorkerThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ioThread, this, CREATE_SUSPENDED, NULL);
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


		ret = connectSession();
		m_isConnected = true;
	}


	if (!ret)
	{
		// 정리
		wprintf(L"connect failed\n");
		releaseClient();

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

	

	disconnectSession();

	releaseClient(); // 스레드 정리 & 대기

	wprintf(L"Disconnected\n");

	return;
}



void CLanClient::releaseClient(void)
{

	// session 연결 끊기

	for(int i = 0; i < m_iocpWorkerNum; i++)
		PostQueuedCompletionStatus(m_hcp, 0, 0, 0);

	WaitForMultipleObjects(m_iocpWorkerNum, m_hWorkerThread, TRUE, INFINITE);

	m_isConnected = false;

	return;
}

unsigned long _stdcall CLanClient::ioThread(void* param)
{
	CLanClient* client = (CLanClient*)param;
	wprintf(L"%d worker thread On...\n", GetCurrentThreadId());
	client->runIoThread();
	wprintf(L"%d IO thread end\n", GetCurrentThreadId());
	return 0;
}

void CLanClient::runIoThread()
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

				break;

			}

		}

		if (cbTransferred == 0 && key == nullptr) // session과 무관한 비동기 요청
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

		if (cbTransferred == 0 || m_disconnect) // Pending 후 I/O 처리 실패 또는 PostQueue
		{
#
			switch(reinterpret_cast<unsigned short>(overlapped))
			{
				case static_cast<unsigned short>(ePost::SEND_PEND):
				{
					trySend();
					break;
				}
				case static_cast<unsigned short>(ePost::RELEASE_PEND): // Release 비동기 요청 처리
				{
					release();
					break;
				}
				case static_cast<unsigned short>(ePost::CANCEL_IO): // Disconnect 처리 후 IO 걸린거 취소 한번 더
				{
					CancelIoEx((HANDLE)m_serverSock, NULL);
					break;
				}
				default: // IO 실패인 경우 연결 종료 처리
				{
					disconnectSession();
					break;
				}
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
						disconnectSession();


					CPacket* packet = CPacket::Alloc();
					int ret_deq = m_recvQ.Dequeue(packet->GetBufferPtrLan(), header.len + sizeof(header));
					packet->MoveWritePos(header.len);
					
					// Net 서버면 Decode 추가


					OnRecv(packet);

					CPacket::Free(packet);
				}
				bool ret_recv = recvPost();

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

				trySend();
			}
			else // send, recv 다 아님(다른 session의 overlapped 전달)
			{
				CrashDump::Crash();
			}
		}

		updateIOCount();
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
				sendPend();
				ret = true;
			}
			else
			{
				packet->SubRef();

				Log(L"SYS", enLOG_LEVEL_ERROR, L"Full Send Q");
			}
		}

	}
	updateIOCount();




	return ret;
}


bool CLanClient::connectSession()
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
		m_disconnect = false;

		m_recvQ.ClearBuffer();

		CreateIoCompletionPort((HANDLE)m_serverSock, m_hcp, (ULONG_PTR)&m_serverSock, 0);

		m_releaseFlag = 0; // 준비 끝


		OnEnterServerJoin();

		recvPost();

		updateIOCount();

	}

	return ret;
}

void CLanClient::disconnectSession()
{
	if (m_disconnect == 0)
	{
		if (InterlockedCompareExchange((LONG*)&m_disconnect, 1, 0) == 0) {
#ifdef TRACE_SESSION
			session->pending_tracer.trace(enDisconnect, session->sock, GetTickCount64());
#endif
			CancelIoEx((HANDLE)m_serverSock, NULL);
			InterlockedIncrement((LONG*)&m_ioCount);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)&m_serverSock, (LPOVERLAPPED)ePost::CANCEL_IO);
			
			

		}
	}
	return;
}

bool CLanClient::recvPost()
{
	DWORD flags = 0;
	bool ret = false;
	DWORD error_code;

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
				
				disconnectSession();
				int io_temp = updateIOCount();
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


	return ret;
}

void CLanClient::sendPost()
{

	if (m_disconnect == 0)
	{

		if ((InterlockedExchange((LONG*)&m_sendFlag, true)) == false) // compare exchange
		{
			int buf_cnt = (int)m_sendQ.GetSize();
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

				SOCKET socket = m_serverSock;
				
				retval = WSASend(socket, wsabuf, buf_cnt, NULL, 0, &m_sendOverlapped, NULL);

				DWORD error_code;
				if (retval == SOCKET_ERROR)
				{
					if ((error_code = WSAGetLastError()) != WSA_IO_PENDING) // 요청 자체가 실패
					{
						if(error_code != WSAECONNRESET)
							Log(L"SYS", enLOG_LEVEL_ERROR, L"Send Failed [%d] session", error_code);

						disconnectSession();
						int io_temp = updateIOCount();

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



	return;
}

void CLanClient::sendPend()
{
	if (InterlockedExchange((LONG*)&m_sendFlag, true) == false)
	{
		InterlockedIncrement((LONG*)&m_ioCount);
		PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)&m_serverSock, (LPOVERLAPPED)ePost::SEND_PEND);
	}
}

void CLanClient::trySend()
{
	m_sendFlag = false;
	if (m_sendQ.GetSize() > 0)
	{
		sendPost();
		sendPend();
	}
}

int CLanClient::updateIOCount()
{
	int temp;
	//tracer.trace(76, session, session->session_id);
	if ((temp = InterlockedDecrement((LONG*)&m_ioCount)) == 0)
	{
		releasePend();
	}
	else if (temp < 0)
	{
		CrashDump::Crash();
	}

	return temp;
}



void CLanClient::releasePend()
{
	unsigned long long flag = *((unsigned long long*)(&m_ioCount));

	if (flag == 0)
	{
		if (InterlockedCompareExchange64((LONG64*)&m_ioCount, 0x100000000, flag) == flag)
		{


			// PostQueue 
			InterlockedIncrement((LONG*)&m_ioCount);
			PostQueuedCompletionStatus(m_hcp, 0, (ULONG_PTR)&m_serverSock, (LPOVERLAPPED)ePost::RELEASE_PEND);

		}
	}

	return;
}


void CLanClient::release()
{

	if (!m_disconnect) CrashDump::Crash(); // 삭제 처리 확인

	OnLeaveServer();

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

	return;
}



void CLanClient::Show()
{
	wprintf(L"-----------------------------------------\n");
	wprintf(L"PacketPool Use: %d\n", CPacket::GetUsePool());


	return;
}

void CLanClient::Log(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szStringFormat, ...)
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

void CLanClient::LogHex(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szLog, BYTE* pByte, int iByteLen)
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

void CLanClient::writeLog()
{
	CLog* log = nullptr;

	while (log == nullptr)
	{
		m_logQueue.Dequeue(&log); // Enqueue 때문에 실패할 수 있음 

	}

	log->WriteFile();
	CLog::Free(log);
}

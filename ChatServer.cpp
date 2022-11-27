#include <Windows.h>

#include <thread>
#include <utility>
#include <vector>
using std::vector;
#include <unordered_map>
using std::unordered_map;

#include "CNetServer.h"
#include "User.h"
#include "MemoryPoolTls.h"

#include "ChatServer.h"
#include "CommonProtocol.h"
#include "ProcPacket.h"
#include "Sector.h"
#include "ObjectPool.h"
#include "CrashDump.h"
#include "ProfileTls.h"
#include "ChatPacket.h"
#include "CLog.h"

#include "MonitorClient.h"

ChatServer g_chatServer;


ChatServer::ChatServer()
{
	for(int i = 0; i < dfUSER_MAP_HASH; i++)
		InitializeSRWLock(&m_userMapCS[i]);

	InitSector();

	m_maxUser = 15000;
	
	m_connectCnt = 0;
	m_loginCnt = 0;
	m_duplicateLogin = 0;
	m_messageTps = 0;


	m_runTimeCheck = true;

	h_timeOutThread = CreateThread(NULL, 0, TimeOutThread, (PVOID)&m_runTimeCheck, 0, NULL);
}


ChatServer::~ChatServer()
{
	/*for (int i = 0; i < dfUSER_MAP_HASH; i++)
		DeleteCriticalSection(&m_userMapCS[i]);*/
	ReleaseSector();

	m_runTimeCheck = false;
	WaitForSingleObject(h_timeOutThread, INFINITE);
	
}

bool ChatServer::OnConnectionRequest(wchar_t* ip, unsigned short port)
{
	//
	
	if (m_loginCnt > m_maxUser)
	{
		return false;
	}

	return true;
}

void ChatServer::OnClientJoin(unsigned long long session_id)
{
	Profile pro = Profile(L"Join");
	CreateUser(session_id);

	InterlockedIncrement(&m_messageTps);
	return;
}

void ChatServer::OnClientLeave(unsigned long long session_id)
{
	Profile pro = Profile(L"Leave");
	DeleteUser(session_id);

	InterlockedIncrement(&m_messageTps);
	return;
}

void ChatServer::OnRecv(unsigned long long session_id, CPacket* recv_packet)
{
	ChatPacket* packet = (ChatPacket*)recv_packet;
	if (!packet->Decode())
	{
		Log(L"SYS", enLOG_LEVEL_DEBUG, L"Packet Error");
		DisconnectSession(session_id);
		return;
	}

	WORD packet_type;
	(*packet) >> packet_type;
	User* user;

	ULONGLONG t = GetTickCount64();

	bool result_proc = false;
	
	if (AcquireUser(session_id, &user))
	{
		long long acc = user->account_no;

		user->old_recv_time = user->last_recv_time;
		user->last_recv_time = GetTickCount64();


		// Packet Proc
		switch (packet_type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
		{
#ifdef dfTRACE_CHAT
			m_chatTracer.trace(10, (PVOID)user->session_id, GetTickCount64());
#endif
			result_proc = PacketProcessor::ProcLogin(user, packet);
			break;
		}
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
#ifdef dfTRACE_CHAT
			m_chatTracer.trace(11, (PVOID)user->session_id, GetTickCount64());
#endif
			result_proc = PacketProcessor::ProcSectorMove(user, packet);
			break;
		}
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
#ifdef dfTRACE_CHAT
			m_chatTracer.trace(12, (PVOID)user->session_id, GetTickCount64());
#endif
			result_proc = PacketProcessor::ProcMessage(user, packet);
			break;
		}
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		{
			result_proc = true;
			break;
		}
		default: // undefined protocol
		{
			// 예외 처리
			break;
		}

		}

		ReleaseUser(user);
	}
	else
	{
		int a = 0;

	}

	if (!result_proc) 
		DisconnectSession(session_id);

	InterlockedIncrement(&m_messageTps);

	return;
}



void ChatServer::OnSend(unsigned long long session_id, int send_size)
{

	return;
}


void ChatServer::OnWorkerThreadBegin()
{

	return;
}

void ChatServer::OnWorkerThreadEnd()
{

	return;
}

void ChatServer::OnError(int errorcode, const wchar_t* msg)
{
	Log(L"SYS", enLOG_LEVEL_ERROR, L"OnError : %d %s", errorcode, msg);

	return;
}

void ChatServer::SetPacket(unsigned char code, unsigned char key)
{
	ChatPacket::SetPacketCode(code);
	ChatPacket::SetPacketKey(key);

}




bool ChatServer::AcquireUser(SS_ID s_id, User** user)
{
	bool ret = true;

	unsigned int idx = s_id % dfUSER_MAP_HASH;

	AcquireSRWLockShared(&m_userMapCS[idx]);
	auto iter = m_userMap[idx].find(s_id);
	if (iter == m_userMap[idx].end())
		CrashDump::Crash(); // 발생하면 결함
	else 
	{
		*user = iter->second;
		(*user)->Lock();
	}
	ReleaseSRWLockShared(&m_userMapCS[idx]);


	return ret;
}

void ChatServer::ReleaseUser(User* user)
{
	user->Unlock();

	return;
}

// accept thread에서 호출
void ChatServer::CreateUser(SS_ID s_id)
{
	User* user = m_userPool.Alloc();
	int idx = s_id % dfUSER_MAP_HASH;


	if (user == nullptr)
		CrashDump::Crash();

	user->Lock();

	user->session_id = s_id;
	user->sector_x = -1;
	user->sector_y = -1;
	user->last_recv_time = GetTickCount64();

#ifdef dfTRACE_CHAT
	m_chatTracer.trace(1, (PVOID)user->session_id, GetTickCount64());
#endif
	// 추가
	AcquireSRWLockExclusive(&m_userMapCS[idx]);
	if (m_userMap[idx].find(s_id) != m_userMap[idx].end())
		CrashDump::Crash();

	m_userMap[idx][s_id] = user;
	ReleaseSRWLockExclusive(&m_userMapCS[idx]);

	user->Unlock();

	InterlockedIncrement(&m_connectCnt);

	return;
}

//Release한 스레드에서 호출
void ChatServer::DeleteUser(SS_ID s_id)
{
	User* user;
	int idx = s_id % dfUSER_MAP_HASH;
#ifdef dfTRACE_CHAT
	m_chatTracer.trace(2, (PVOID)s_id, GetTickCount64());
#endif
	AcquireSRWLockExclusive(&m_userMapCS[idx]);
	auto iter = m_userMap[idx].find(s_id);
	if (iter == m_userMap[idx].end())
		CrashDump::Crash();
	user = iter->second;


	m_userMap[idx].erase(s_id);
	ReleaseSRWLockExclusive(&m_userMapCS[idx]);

	user->Lock();
	if (user->session_id != s_id)
	{
		CrashDump::Crash();
	}

	if (user->is_in_sector)
	{
		AcquireSRWLockExclusive(&g_SectorLock[user->sector_y][user->sector_x]);
		Sector_RemoveUser(user);
		ReleaseSRWLockExclusive(&g_SectorLock[user->sector_y][user->sector_x]);
		user->is_in_sector = false;
	}

	user->sector_x = SECTOR_MAX_X;
	user->sector_y = SECTOR_MAX_Y;


	if (user->is_login) {
		InterlockedDecrement(&m_loginCnt);
		user->is_login = false;
	}
	else
		InterlockedDecrement(&m_connectCnt);

	user->session_id = -1;
	user->account_no = -1;

	user->Unlock();

	m_userPool.Free(user);

	return;

}

void ChatServer::SendMessageUni(CPacket* packet, User* user)
{
	// user lock?
	// user delete 로직 탈 수 있다.. 

	SendPacket(user->session_id, packet);

	return;
}

void ChatServer::SendMessageSector(CPacket* packet, int sector_x, int sector_y, User* user)
{
	list<User*>& sector = g_SectorList[sector_y][sector_x];
	User* target_user;

	for (auto iter = sector.begin(); iter != sector.end();)
	{
		target_user = (*iter);
		++iter;  // 삭제 가능성 있으므로 미리 옮겨둠
		if (user != target_user) {
			SendMessageUni(packet, target_user);
		}
		else
		{
			SendMessageUni(packet, target_user);
		}
	}

	return;
}


void ChatServer::SendMessageAround(CPacket* packet, User* user)
{
	SectorAround sect_around;
	int cnt;


	GetSectorAround(user->sector_x, user->sector_y, &sect_around);

	LockSectorAround(&sect_around);

	for (cnt = 0; cnt < sect_around.count; cnt++)
	{
		SendMessageSector(packet, sect_around.around[cnt].x, sect_around.around[cnt].y, user);
	}

	UnlockSectorAround(&sect_around);

	return;
}


void ChatServer::Show()
{
	CNetServer::Show();

	int user_cnt = 0;

	for (int i = 0; i < dfUSER_MAP_HASH; i++)
	{
		user_cnt += m_userMap[i].size();
	}

	unsigned int message_tps = InterlockedExchange(&m_messageTps, 0);
	wprintf(L"Connect : %d\n"
		L"Login : %d\n"
		L"Duplicated login proc : %d\n"
		L"Message TPS : %d\n"
		L"User : %d\n",
		m_connectCnt, m_loginCnt, m_duplicateLogin, message_tps, user_cnt);


}

void ChatServer::SendMonitor(int time_stamp)
{
	/*dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN				= 30,		// 에이전트 ChatServer 실행 여부 ON / OFF
	dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU				= 31,		// 에이전트 ChatServer CPU 사용률
	dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM				= 32,		// 에이전트 ChatServer 메모리 사용 MByte
	dfMONITOR_DATA_TYPE_CHAT_SESSION				= 33,		// 채팅서버 세션 수 (컨넥션 수)
	dfMONITOR_DATA_TYPE_CHAT_PLAYER					= 34,		// 채팅서버 인증성공 사용자 수 (실제 접속자)
	dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS				= 35,		// 채팅서버 UPDATE 스레드 초당 초리 횟수
	dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL			= 36,		// 채팅서버 패킷풀 사용량
	dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL			= 37,		// 채팅서버 UPDATE MSG 풀 사용량*/
	//------------------------------------------------------------
	// 서버가 모니터링서버로 데이터 전송
	// 각 서버는 자신이 모니터링중인 수치를 1초마다 모니터링 서버로 전송.
	//
	// 서버의 다운 및 기타 이유로 모니터링 데이터가 전달되지 못할떄를 대비하여 TimeStamp 를 전달한다.
	// 이는 모니터링 클라이언트에서 계산,비교 사용한다.
	// 
	//	{
	//		WORD	Type
	//
	//		BYTE	DataType				// 모니터링 데이터 Type 하단 Define 됨.
	//		int		DataValue				// 해당 데이터 수치.
	//		int		TimeStamp				// 해당 데이터를 얻은 시간 TIMESTAMP  (time() 함수)
	//										// 본래 time 함수는 time_t 타입변수이나 64bit 로 낭비스러우니
	//										// int 로 캐스팅하여 전송. 그래서 2038년 까지만 사용가능
	//	}
	//
	//------------------------------------------------------------
	
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;


	

}

void ChatServer::CheckTimeOut()
{
	int iCnt;

	for (iCnt = 0; iCnt < dfUSER_MAP_HASH; iCnt++)
	{
		AcquireSRWLockShared(&g_chatServer.m_userMapCS[iCnt]);
	}


	for (iCnt = 0; iCnt < dfUSER_MAP_HASH; iCnt++)
	{
		for (auto& iter : m_userMap[iCnt])
		{
			User* user = iter.second;
			if (GetTickCount64() - user->GetLastRecvTime() >= 40000) {
				ULONG64 lastTick = user->last_recv_time;
				ULONG64 nowTick;
				user->last_recv_time = nowTick = GetTickCount64();
				unsigned long long s_id = user->session_id;
				// disconnect
				DisconnectSession(user->session_id);
				Log(L"Chat", enLOG_LEVEL_DEBUG, L"Time Out session: %lld %lld %lld\n", s_id, lastTick, nowTick);

			}
		}
	}

	for (iCnt = 0; iCnt < dfUSER_MAP_HASH; iCnt++)
	{
		ReleaseSRWLockShared(&g_chatServer.m_userMapCS[iCnt]);
	}




}

DWORD TimeOutThread(PVOID param)
{
	bool* runTimeCheck = (bool*)param;

	while (*runTimeCheck)
	{
		Sleep(1000);
		g_chatServer.CheckTimeOut();
	}

	return 0;
}
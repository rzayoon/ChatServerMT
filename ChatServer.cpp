

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#include <cpp_redis/cpp_redis>

#include <Windows.h>

#include <thread>
#include <utility>
#include <vector>
using std::vector;
#include <unordered_map>
using std::unordered_map;


#include "MonitorClient.h"
#include "CCpuUsage.h"
#include "CPDH.h"
#include "CrashDump.h"

#include "session.h"
#include "CNetServer.h"
#include "User.h"
#include "MemoryPoolTls.h"
#include "ChatServer.h"

#include "CommonProtocol.h"
#include "ProcPacket.h"
#include "Sector.h"
#include "ObjectPool.h"
#include "ProfileTls.h"

#include "CLog.h"

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
	m_collectMsgTPS = 0;

	m_pdh.Init();

}


ChatServer::~ChatServer()
{
	/*for (int i = 0; i < dfUSER_MAP_HASH; i++)
		DeleteCriticalSection(&m_userMapCS[i]);*/
	ReleaseSector();

	
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

void ChatServer::OnRecv(unsigned long long session_id, CPacket* packet)
{
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
			if (!result_proc)
			{
				Log(L"Recv", enLOG_LEVEL_DEBUG, L"Login Proc fail account %d", user->account_no);
			}
			break;
		}
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
#ifdef dfTRACE_CHAT
			m_chatTracer.trace(11, (PVOID)user->session_id, GetTickCount64());
#endif
			result_proc = PacketProcessor::ProcSectorMove(user, packet);
			if (!result_proc)
			{
				Log(L"Recv", enLOG_LEVEL_DEBUG, L"SectorMove Proc fail account %d", user->account_no);
		}
			break;
		}
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
#ifdef dfTRACE_CHAT
			m_chatTracer.trace(12, (PVOID)user->session_id, GetTickCount64());
#endif
			result_proc = PacketProcessor::ProcMessage(user, packet);
			if (!result_proc)
			{
				Log(L"Recv", enLOG_LEVEL_DEBUG, L"Chat Message fail account %d", user->account_no);
			}
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
			Log(L"Recv", enLOG_LEVEL_DEBUG, L"Undefined Packet account %d", user->account_no);
			break;
		}

		}

		ReleaseUser(user);
	}
	else
	{
		
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

void ChatServer::OnClientTimeout(unsigned long long session_id)
{
	CrashDump::Crash();

	Log(L"Disconnect", enLOG_LEVEL_DEBUG, L"Timeout session %lld", session_id);

	DisconnectSession(session_id);

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

	
	wprintf(L"Connect : %d\n"
		L"Login : %d\n"
		L"Duplicated login proc : %d\n"
		L"Message TPS : %d\n"
		L"User : %d\n",
		m_connectCnt, m_loginCnt, m_duplicateLogin, m_collectMsgTPS, user_cnt);

	m_CpuTime.Show();
}


void ChatServer::Collect()
{
	m_collectMsgTPS = InterlockedExchange(&m_messageTps, 0);
	m_CpuTime.UpdateCpuTime();
	m_pdh.Collect();
}

bool ChatServer::ConnectMonitor()
{
	if(!m_monitorCli.IsConnected())
		return m_monitorCli.Connect(m_monitorIP, m_monitorPort, 1, 1, 1);
	return false;
}

void ChatServer::SendMonitor(int time_stamp)
{
	int use_pool = CPacket::GetUsePool();

	//Run
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, 1, time_stamp);
	// CPU 사용율
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, m_CpuTime.ProcessTotal(), time_stamp);
	// 메모리 사용 MByte
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, m_pdh.GetPrivateMBytes(), time_stamp);
	// 세션 수
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SESSION, GetSessionCount(), time_stamp);
	// 로그인 유저 수
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_PLAYER, m_loginCnt, time_stamp);
	// 초당 처리 수
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, m_collectMsgTPS, time_stamp);
	// 패킷 풀 사용량
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, use_pool, time_stamp);

	return;
}

void ChatServer::SetMonitorClientInfo(const wchar_t* serverIp, unsigned short port)
{
	memcpy(m_monitorIP, serverIp, sizeof(m_monitorIP));
	m_monitorPort = port;

	return;

}


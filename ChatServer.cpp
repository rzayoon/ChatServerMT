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
#include "TextParser.h"
#include "CLog.h"

ChatServer g_chatServer;


ChatServer::ChatServer()
{
	
}


ChatServer::~ChatServer()
{

	
}

bool ChatServer::Start()
{
	for (int i = 0; i < dfUSER_MAP_HASH; i++)
		InitializeSRWLock(&m_userMapCS[i]);

	InitSector();

	m_maxUser = 15000;

	m_connectCnt = 0;
	m_loginCnt = 0;
	m_duplicateLogin = 0;
	m_messageTps = 0;
	m_collectMsgTPS = 0;

	m_runTimeCheck = true;

	m_pdh.Init();

	h_timeOutThread = CreateThread(NULL, 0, TimeOutThread, (PVOID)&m_runTimeCheck, 0, NULL);

	char ip[16];
	int port;
	int worker;
	int max_worker;
	int max_user;
	int max_session;
	int packet_code;
	int packet_key;
	int nagle;


	TextParser parser;
	if (!parser.LoadFile("Config.ini")) return 1;
	wchar_t wip[16];

	parser.GetStringValue("ServerBindIP", ip, 16);
	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);
	parser.GetValue("ServerBindPort", &port);
	parser.GetValue("IOCPWorkerThread", &worker);
	parser.GetValue("IOCPActiveThread", &max_worker);
	parser.GetValue("MaxUser", &max_user);
	parser.GetValue("MaxSession", &max_session);
	parser.GetValue("PacketCode", &packet_code);
	parser.GetValue("PacketKey", &packet_key);

	parser.GetValue("Nagle", &nagle);

	bool ret = CNetServer::Start(wip, port, worker, max_worker, max_session, nagle, packet_key, packet_code);
	if (!ret) return false;

	parser.GetStringValue("MonitorIP", ip, 16);
	MultiByteToWideChar(CP_ACP, 0, ip, 16, m_monitorIP, 16);
	parser.GetValue("MonitorPort", &port);
	m_monitorPort = port;
	parser.GetValue("ClientIOCPWorker", &m_monitorWorker);
	parser.GetValue("ClientIOCPActive", &m_monitorActive);

	ConnectMonitor();

	return true;
}

void ChatServer::Stop()
{
	/*for (int i = 0; i < dfUSER_MAP_HASH; i++)
	DeleteCriticalSection(&m_userMapCS[i]);*/
	ReleaseSector();

	if (m_monitorCli.IsConnected())
		m_monitorCli.Disconnect();

	m_runTimeCheck = false;
	WaitForSingleObject(h_timeOutThread, INFINITE);


	return;
}

bool ChatServer::OnConnectionRequest(const wchar_t* ip, unsigned short port)
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
			// ���� ó��
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




bool ChatServer::AcquireUser(SS_ID s_id, User** user)
{
	bool ret = true;

	unsigned int idx = s_id % dfUSER_MAP_HASH;

	AcquireSRWLockShared(&m_userMapCS[idx]);
	auto iter = m_userMap[idx].find(s_id);
	if (iter == m_userMap[idx].end())
		CrashDump::Crash(); // �߻��ϸ� ����
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

// accept thread���� ȣ��
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
	// �߰�
	AcquireSRWLockExclusive(&m_userMapCS[idx]);
	if (m_userMap[idx].find(s_id) != m_userMap[idx].end())
		CrashDump::Crash();

	m_userMap[idx][s_id] = user;
	ReleaseSRWLockExclusive(&m_userMapCS[idx]);

	user->Unlock();

	InterlockedIncrement(&m_connectCnt);

	return;
}

//Release�� �����忡�� ȣ��
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
		AcquireSRWLockExclusive(&m_accountMapSRW);
		m_accountMap[user->account_no] = 0;
		// account ������ �����ڵ��..
		ReleaseSRWLockExclusive(&m_accountMapSRW);

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
	// user delete ���� Ż �� �ִ�.. 

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
		++iter;  // ���� ���ɼ� �����Ƿ� �̸� �Űܵ�
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
			ULONG64 nowTick = GetTickCount64();
			ULONG64 lastTick = user->last_recv_time;
			unsigned long long s_id = user->session_id;
			if ((LONG64)(nowTick - lastTick) >= 40000) {
				
				// disconnect
				DisconnectSession(user->session_id);
				Log(L"Chat", enLOG_LEVEL_DEBUG, L"Time Out session: %lld %lld %lld\n", s_id, nowTick, lastTick);

			}
		}
	}

	for (iCnt = 0; iCnt < dfUSER_MAP_HASH; iCnt++)
	{
		ReleaseSRWLockShared(&g_chatServer.m_userMapCS[iCnt]);
	}




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
		return m_monitorCli.Connect(m_monitorIP, m_monitorPort, m_monitorWorker, m_monitorActive, true);
	return false;
}

void ChatServer::SendMonitor(int time_stamp)
{
	int use_pool = CPacket::GetUsePool();

	//Run
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, 1, time_stamp);
	// CPU �����
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, m_CpuTime.ProcessTotal(), time_stamp);
	// �޸� ��� MByte
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, m_pdh.GetPrivateMBytes(), time_stamp);
	// ���� ��
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SESSION, GetSessionCount(), time_stamp);
	// �α��� ���� ��
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_PLAYER, m_loginCnt, time_stamp);
	// �ʴ� ó�� ��
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, m_collectMsgTPS, time_stamp);
	// ��Ŷ Ǯ ��뷮
	m_monitorCli.SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, use_pool, time_stamp);

	return;
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
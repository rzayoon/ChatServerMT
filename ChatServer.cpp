#include <thread>
#include <utility>
#include <unordered_map>

#include "MemoryPoolTls.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "ProcPacket.h"
#include "Sector.h"
#include "ObjectPool.h"
#include "CrashDump.h"
#include "ProfileTls.h"




ChatServer::ChatServer()
{
	for(int i = 0; i < dfUSER_MAP_HASH; i++)
		InitializeCriticalSection(&m_userMapCS[i]);

	InitSector();

	m_maxUser = 15000;

	m_connectCnt = 0;
	m_loginCnt = 0;
	m_duplicateLogin = 0;
	m_messageTps = 0;

	runningWorker = 0;
}


ChatServer::~ChatServer()
{
	for (int i = 0; i < dfUSER_MAP_HASH; i++)
		DeleteCriticalSection(&m_userMapCS[i]);
	ReleaseSector();

}

bool ChatServer::OnConnectionRequest(wchar_t* ip, unsigned short port)
{
	// �����ϰ� login cnt ���� ������ accept������� �� �ö����� ����
	
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

	if (!AcquireUser(session_id, &user))
		CrashDump::Crash();

	long long acc = user->account_no;

	user->old_recv_time = user->last_recv_time;
	user->last_recv_time = GetTickCount64();

	bool result_proc = false;

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
		//  �ʿ� ����
		break;
	}
	default: // undefined protocol
	{
		// ���� ó��
		break;
	}

	}

	if (!result_proc) 
		DisconnectSession(user->session_id);

	ReleaseUser(user);
	InterlockedIncrement(&m_messageTps);

	return;
}



void ChatServer::OnSend(unsigned long long session_id, int send_size)
{

	return;
}


void ChatServer::OnWorkerThreadBegin()
{
	InterlockedIncrement((LONG*)&runningWorker);

	return;
}

void ChatServer::OnWorkerThreadEnd()
{
	InterlockedDecrement((LONG*)&runningWorker);

	return;
}

void ChatServer::OnError(int errorcode, const wchar_t* msg)
{

	return;
}




bool ChatServer::AcquireUser(SS_ID s_id, User** user)
{
	bool ret = true;

	unsigned int idx = s_id % dfUSER_MAP_HASH;

	EnterCriticalSection(&m_userMapCS[idx]);
	auto iter = m_userMap[idx].find(s_id);
	if (iter == m_userMap[idx].end())
		CrashDump::Crash();
	*user = iter->second;
	(*user)->Lock();

	LeaveCriticalSection(&m_userMapCS[idx]);


	return true;
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
	EnterCriticalSection(&m_userMapCS[idx]);
	if (m_userMap[idx].find(s_id) != m_userMap[idx].end())
		CrashDump::Crash();

	m_userMap[idx][s_id] = user;
	LeaveCriticalSection(&m_userMapCS[idx]);

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
	EnterCriticalSection(&m_userMapCS[idx]);
	auto iter = m_userMap[idx].find(s_id);
	if (iter == m_userMap[idx].end())
		CrashDump::Crash();
	user = iter->second;


	m_userMap[idx].erase(s_id);
	LeaveCriticalSection(&m_userMapCS[idx]);

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

	unsigned int message_tps = InterlockedExchange(&m_messageTps, 0);
	wprintf(L"Connect : %d\n"
		L"Login : %d\n"
		L"Duplicated login proc : %d\n"
		L"Message TPS : %d\n"
		L"Running WorkerThread : %d\n",
		m_connectCnt, m_loginCnt, m_duplicateLogin, message_tps, runningWorker);


}
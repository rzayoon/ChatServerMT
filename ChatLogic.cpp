#include <utility>


#include "ChatServer.h"
#include "MemoryPoolTls.h"
#include "ObjectPool.h"
#include "ChatLogic.h"
#include "LockFreeQueue.h"
#include "User.h"
#include "CommonProtocol.h"
#include "ProcPacket.h"
#include "Sector.h"
#include "CrashDump.h"
#include "ProfileTls.h"

MemoryPoolTls<User> g_UserPool(500);

unordered_map<SS_ID, User*> g_UserMap[dfUSER_MAP_HASH];
CRITICAL_SECTION g_UserMapCS[dfUSER_MAP_HASH];

alignas(64) unsigned int g_connect_cnt;
alignas(64) unsigned int g_login_cnt;
alignas(64) unsigned int g_duplicate_login;
alignas(64) unsigned int g_message_tps;

Tracer g_Tracer;

bool AcquireUser(SS_ID s_id, User** user)
{
	bool ret = true;

	unsigned int idx = s_id % dfUSER_MAP_HASH;

	EnterCriticalSection(&g_UserMapCS[idx]);
	auto iter = g_UserMap[idx].find(s_id);
	if (iter == g_UserMap[idx].end())
		CrashDump::Crash();
	*user = iter->second;
	(*user)->Lock();

	LeaveCriticalSection(&g_UserMapCS[idx]);


	return true;
}

void ReleaseUser(User* user)
{
	user->Unlock();

	return;
}

// accept thread에서 호출
void CreateUser(SS_ID s_id)
{
	User* user = g_UserPool.Alloc();
	int idx = s_id % dfUSER_MAP_HASH;


	if (user == nullptr)
		CrashDump::Crash();

	user->Lock();

	user->session_id = s_id;
	user->sector_x = -1;
	user->sector_y = -1;
	user->last_recv_time = GetTickCount64();

	
	g_Tracer.trace(1, (PVOID)user->session_id, GetTickCount64());

	// 추가
	EnterCriticalSection(&g_UserMapCS[idx]);
	if (g_UserMap[idx].find(s_id) != g_UserMap[idx].end())
		CrashDump::Crash();

	g_UserMap[idx][s_id] = user;
	LeaveCriticalSection(&g_UserMapCS[idx]);

	user->Unlock();

	InterlockedIncrement(&g_connect_cnt);

	return;
}

//Release한 스레드에서 호출
void DeleteUser(SS_ID s_id)
{
	User* user;
	int idx = s_id % dfUSER_MAP_HASH;

	g_Tracer.trace(2, (PVOID)s_id, GetTickCount64());

	EnterCriticalSection(&g_UserMapCS[idx]);
	auto iter = g_UserMap[idx].find(s_id);
	if (iter == g_UserMap[idx].end())
		CrashDump::Crash();
	user = iter->second;


	g_UserMap[idx].erase(s_id);
	LeaveCriticalSection(&g_UserMapCS[idx]);

	user->Lock();
	if (user->session_id != s_id)
	{
		user->Unlock();
		return;
	}

	if (user->is_in_sector)
	{
		Sector_RemoveUser(user);
		user->is_in_sector = false;
	}

	user->sector_x = SECTOR_MAX_X;
	user->sector_y = SECTOR_MAX_Y;




	if (user->is_login) {
		InterlockedDecrement(&g_login_cnt);
		user->is_login = false;
	}
	else
		InterlockedDecrement(&g_connect_cnt);

	user->session_id = -1;
	user->account_no = -1;

	user->Unlock();

	g_UserPool.Free(user);

	return;

}

void SendMessageUni(CPacket* packet, User* user)
{
	// user lock?
	// user delete 로직 탈 수 있다.. 

	g_server.SendPacket(user->session_id, packet);

	return;
}

void SendMessageSector(CPacket* packet, int sector_x, int sector_y)
{
	list<User*>& sector = g_SectorList[sector_y][sector_x];
	User* user;

	for (auto iter = sector.begin(); iter != sector.end();)
	{
		user = (*iter);
		++iter;  // 삭제 가능성 있으므로 미리 옮겨둠
		SendMessageUni(packet, user);
	}

	return;
}


void SendMessageAround(CPacket* packet, User* user)
{
	SectorAround sect_around;
	int cnt;


	GetSectorAround(user->sector_x, user->sector_y, &sect_around);

	LockSectorAround(&sect_around);

	for (cnt = 0; cnt < sect_around.count; cnt++)
	{
		SendMessageSector(packet, sect_around.around[cnt].x, sect_around.around[cnt].y);
	}

	UnlockSectorAround(&sect_around);

	return;
}
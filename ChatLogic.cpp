#include <utility>


#include "ChatServer.h"
#include "MemoryPoolTls.h"
#include "ObjectPool.h"
#include "ChatLogic.h"
#include "LockFreeQueue.h"
#include "JOB.h"
#include "User.h"
#include "CommonProtocol.h"
#include "ProcPacket.h"
#include "Sector.h"
#include "CrashDump.h"

MemoryPoolTls<User> g_UserPool(300);

unordered_map<SS_ID, User*> g_UserMap;
SRWLOCK g_UserMapSRW;

alignas(64) unsigned int g_connect_cnt;
alignas(64) unsigned int g_login_cnt;

Tracer g_Tracer;

bool AcquireUser(SS_ID s_id, User** user)
{
	bool ret = true;

	AcquireSRWLockShared(&g_UserMapSRW);
	auto iter = g_UserMap.find(s_id);
	if (iter == g_UserMap.end())
	{
		ReleaseSRWLockShared(&g_UserMapSRW);
		return false;
	}
	*user = iter->second;
	(*user)->Lock();

	ReleaseSRWLockShared(&g_UserMapSRW);


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

	user->Lock();

	user->session_id = s_id;
	user->sector_x = -1;
	user->sector_y = -1;
	user->last_recv_time = GetTickCount64();

	
	g_Tracer.trace(1, (PVOID)user->session_id);

	// 추가
	AcquireSRWLockExclusive(&g_UserMapSRW);
	g_UserMap[s_id] = user;
	ReleaseSRWLockExclusive(&g_UserMapSRW);

	user->Unlock();

	InterlockedIncrement(&g_connect_cnt);

	return;
}

//Release한 스레드에서 호출
void DeleteUser(SS_ID s_id)
{
	User* user;

	AcquireSRWLockExclusive(&g_UserMapSRW);
	auto iter = g_UserMap.find(s_id);
	if (iter == g_UserMap.end())
		CrashDump::Crash();
	user = iter->second;

	user->Lock();

	g_UserMap.erase(s_id);
	ReleaseSRWLockExclusive(&g_UserMapSRW);


	if (user->is_in_sector)
	{
		Sector_RemoveUser(user);
		user->is_in_sector = false;
	}

	user->sector_x = SECTOR_MAX_X;
	user->sector_y = SECTOR_MAX_Y;



	g_Tracer.trace(2, user);

	if (user->is_login) {
		InterlockedDecrement(&g_login_cnt);
		user->is_login = false;
	}
	else
		InterlockedDecrement(&g_connect_cnt);

	user->session_id = -1;
	user->account_no = -1;

	ReleaseUser(user);

	g_UserPool.Free(user);

	return;

}

void SendMessageUni(CPacket* packet, User* user)
{
	// user lock?

	g_server.SendPacket(user->session_id, packet);

	return;
}

void SendMessageSector(CPacket* packet, int sector_x, int sector_y)
{
	auto& sector = g_SectorList[sector_y][sector_x];

	for (auto& user : sector)
	{
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
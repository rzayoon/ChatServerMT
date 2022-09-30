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

MemoryPoolTls<User> g_UserPool(500, true);

unordered_map<SS_ID, User*> g_UserMap;
SRWLOCK g_UserMapSRW;

alignas(64) unsigned int g_connect_cnt;
alignas(64) unsigned int g_login_cnt;

Tracer g_Tracer;

bool FindUser(SS_ID s_id, User** user)
{
	bool ret = true;

	AcquireSRWLockShared(&g_UserMapSRW);
	auto iter = g_UserMap.find(s_id);
	if (iter == g_UserMap.end())
	{
		ReleaseSRWLockShared(&g_UserMapSRW);
		return false;
	}
	ReleaseSRWLockShared(&g_UserMapSRW);

	*user = iter->second;

	return true;
}

// accept thread에서 호출
void CreateUser(SS_ID s_id)
{
	User* user = g_UserPool.Alloc();

	user->session_id = s_id;
	user->last_recv_time = GetTickCount64();

	
	g_Tracer.trace(1, (PVOID)user->session_id);

	// 추가
	AcquireSRWLockExclusive(&g_UserMapSRW);
	g_UserMap[s_id] = user;
	ReleaseSRWLockExclusive(&g_UserMapSRW);

	InterlockedIncrement(&g_connect_cnt);

}

//Release한 스레드에서 호출
void DeleteUser(SS_ID s_id)
{
	User* user;

	if (!FindUser(s_id, &user))
	{
		g_Tracer.trace(3, (PVOID)s_id);
		return;
	}

	if (user->is_in_sector)
	{
		Sector_RemoveUser(user);
	}

	AcquireSRWLockExclusive(&g_UserMapSRW);
	g_UserMap.erase(s_id);
	ReleaseSRWLockExclusive(&g_UserMapSRW);

	g_Tracer.trace(2, user);

	if (user->is_login)
		InterlockedDecrement(&g_login_cnt);
	else
		InterlockedDecrement(&g_connect_cnt);

	g_UserPool.Free(user);

	return;

}

void SendMessageUni(CPacket* packet, User* user)
{
	//User* user;
	//if (!FindUser(sid, &user))
	//{

	//	return;
	//}

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
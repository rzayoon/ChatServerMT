
#include <list>
using std::list;
#include "CrashDump.h"

#include "Tracer.h"
#include "User.h"
#include "Sector.h"

#include "ProfileTls.h"

list<User*> g_SectorList[SECTOR_MAX_Y][SECTOR_MAX_X];
SRWLOCK g_SectorLock[SECTOR_MAX_Y][SECTOR_MAX_X];
//Tracer g_SecTracer;

void Sector_AddUser(User* user)
{
	Profile pro(L"AddUser");

	if (user->IsInSector())
		CrashDump::Crash();

	short sector_y(user->GetSectorY()), sector_x(user->GetSectorX());
	list<User*>& sector = g_SectorList[sector_y][sector_x];

#ifdef REMEMBER_ITER
	user->SetSectorIter( sector.insert(sector.end(), user) );
#else
	sector.push_back(user);
#endif

	__int64 acc_no = user->GetAccountNo();
	//g_SecTracer.trace(enSECTORADD, acc_no, sector_y, sector_x);

	user->SetInSector();

	return;
}

void Sector_RemoveUser(User* user)
{
	Profile pro(L"RemoveUser");

	if (!user->IsInSector())
		return;

	int sector_y = user->GetSectorY();
	int sector_x = user->GetSectorX();

	list<User*>& sector = g_SectorList[sector_y][sector_x];
#ifdef REMEMBER_ITER
	sector.erase(user->GetSectorIter());
#else
	auto iter_end = sector.end();

	for (auto iter = sector.begin(); iter != iter_end; ++iter)
	{
		if ((*iter) == user)
		{
			sector.erase(iter);
			break;
		}
	}
#endif
	//g_SecTracer.trace(enSECTORDEL, acc_no, sector_y, sector_x);

	user->ResetInSector();

	return;
}

void GetSectorAround(int sector_x, int sector_y, SectorAround* sector_around)
{
	int cnt_x, cnt_y;

	sector_x--;
	sector_y--;

	sector_around->count = 0;

	for (cnt_y = 0; cnt_y < 3; cnt_y++)
	{
		int temp_y = sector_y + cnt_y;
		if (temp_y < 0 || temp_y >= SECTOR_MAX_Y)
			continue;

		for (cnt_x = 0; cnt_x < 3; cnt_x++)
		{
			int temp_x = sector_x + cnt_x;
			if (temp_x < 0 || temp_x >= SECTOR_MAX_X)
				continue;

			sector_around->around[sector_around->count].x = temp_x;
			sector_around->around[sector_around->count].y = temp_y;
			sector_around->count++;
		}
		

	}

	return;

}

void LockSectorAround(const SectorAround* sector_around)
{
	for (int cnt = 0; cnt < sector_around->count; cnt++)
	{
		DWORD y = sector_around->around[cnt].y;
		DWORD x = sector_around->around[cnt].x;


		AcquireSRWLockShared(&g_SectorLock[y][x]);
		//g_SecTracer.trace(enSECTORLOCKSHARED, 0, y, x);
	}

	return;
}

void UnlockSectorAround(const SectorAround* sector_around)
{
	for (int cnt = 0; cnt < sector_around->count; cnt++)
	{
		DWORD y = sector_around->around[cnt].y;
		DWORD x = sector_around->around[cnt].x;

		ReleaseSRWLockShared(&g_SectorLock[sector_around->around[cnt].y][sector_around->around[cnt].x]);
		//g_SecTracer.trace(enSECTORUNLOCKSHARED, 0, y, x);
	}

	return;
}

void InitSector()
{
	for (int i = 0; i < SECTOR_MAX_Y; i++)
		for (int j = 0; j < SECTOR_MAX_X; j++)
			InitializeSRWLock(&g_SectorLock[i][j]);

	return;
}

void ReleaseSector()
{
	/*for (int i = 0; i < SECTOR_MAX_Y; i++)
		for (int j = 0; j < SECTOR_MAX_X; j++)
			DeleteCriticalSection(&g_SectorLock[i][j]);*/

	return;
}

//void LockExclusiveSector(const unsigned short y, const unsigned short x)
//{
//	EnterCriticalSection(&g_SectorLock[y][x]);
//	return;
//}
//
//void UnlockSector(const unsigned short y, const unsigned short x)
//{
//	LeaveCriticalSection(&g_SectorLock[y][x]);
//	return;
//}


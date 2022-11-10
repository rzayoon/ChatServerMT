

#include "Sector.h"
#include "User.h"
#include "CrashDump.h"

list<User*> g_SectorList[SECTOR_MAX_Y][SECTOR_MAX_X];
SRWLOCK g_SectorLock[SECTOR_MAX_Y][SECTOR_MAX_X];


void Sector_AddUser(User* user)
{
	if (user->is_in_sector)
		CrashDump::Crash();

	unsigned short sector_y(user->sector_y), sector_x(user->sector_x);

	g_SectorList[sector_y][sector_x].push_back(user);

	user->is_in_sector = true;

	return;
}

void Sector_RemoveUser(User* user)
{
	if (!user->is_in_sector)
		CrashDump::Crash();

	int sector_y = user->sector_y;
	int sector_x = user->sector_x;

	list<User*>& sector = g_SectorList[sector_y][sector_x];

	for (auto iter = sector.begin(); iter != sector.end(); ++iter)
	{
		if ((*iter) == user)
		{
			sector.erase(iter);
			break;
		}


	}

	user->is_in_sector = false;

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
		AcquireSRWLockShared(&g_SectorLock[sector_around->around[cnt].y][sector_around->around[cnt].x]);
	}

	return;
}

void UnlockSectorAround(const SectorAround* sector_around)
{
	for (int cnt = 0; cnt < sector_around->count; cnt++)
	{
		ReleaseSRWLockShared(&g_SectorLock[sector_around->around[cnt].y][sector_around->around[cnt].x]);
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


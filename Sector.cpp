

#include "ChatLogic.h"
#include "Sector.h"
#include "User.h"


list<User*> g_SectorList[SECTOR_MAX_Y][SECTOR_MAX_X];
SRWLOCK g_SectorLock[SECTOR_MAX_Y][SECTOR_MAX_X];


void Sector_AddUser(User* user)
{
	unsigned short sector_y(user->sector_y), sector_x(user->sector_x);

	LockSectorExclusive(sector_y, sector_x);
	g_SectorList[sector_y][sector_x].push_back(user);
	UnlockSectorExclusive(sector_y, sector_x);

	return;
}

void Sector_RemoveUser(User* user)
{
	int sector_y = user->sector_y;
	int sector_x = user->sector_x;

	LockSectorExclusive(sector_y, sector_x);
	list<User*>& sector = g_SectorList[sector_y][sector_x];

	for (auto iter = sector.begin(); iter != sector.end(); ++iter)
	{
		if ((*iter) == user)
		{
			sector.erase(iter);
			break;
		}


	}
	UnlockSectorExclusive(sector_y, sector_x);


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

void LockSectorAround(SectorAround* sector_around)
{
	for (int cnt = 0; cnt < sector_around->count; cnt++)
	{
		LockSectorShared(sector_around->around[cnt].y, sector_around->around[cnt].x);
	}

	return;
}

void UnlockSectorAround(SectorAround* sector_around)
{
	for (int cnt = 0; cnt < sector_around->count; cnt++)
	{
		UnlockSectorShared(sector_around->around[cnt].y, sector_around->around[cnt].x);
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

void LockSectorExclusive(unsigned short y, unsigned short x)
{
	AcquireSRWLockExclusive(&g_SectorLock[y][x]);
	return;
}

void UnlockSectorExclusive(unsigned short y, unsigned short x)
{
	ReleaseSRWLockExclusive(&g_SectorLock[y][x]);
	return;
}

void LockSectorShared(unsigned short y, unsigned short x)
{
	AcquireSRWLockShared(&g_SectorLock[y][x]);
	return;
}

void UnlockSectorShared(unsigned short y, unsigned short x)
{
	ReleaseSRWLockShared(&g_SectorLock[y][x]);
	return;
}

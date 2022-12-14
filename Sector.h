#pragma once
#include <list>
using std::list;

#include "User.h"

#define SECTOR_MAX_X 50
#define SECTOR_MAX_Y 50


struct SectorPos
{
	DWORD x;
	DWORD y;
};

struct SectorAround
{
	int count;
	SectorPos around[9];

};

/// <summary>
/// User 내 저장된 섹터 위치 멤버에 맞는 Sector List에 User 추가
/// 
/// </summary>
/// <param name="user"></param>
void Sector_AddUser(User* user);

/// <summary>
/// 현재 user가 위치한 Sector에서 user 제거
/// 
/// </summary>
/// <param name="user"></param>
void Sector_RemoveUser(User* user);

/// <summary>
/// 
/// </summary>
/// <param name="sector_x"> 주변 섹터 얻을 Sector의 x 좌표 </param>
/// <param name="sector_y"> 주변 섹터 얻을 Sector의 y 좌표 </param>
/// <param name="sector_around"> [Out] 주변 섹터 정보 </param>
void GetSectorAround(int sector_x, int sector_y, SectorAround* sector_around);

/// <summary>
/// 주변 섹터에 대한 Lock 획득
/// </summary>
/// <param name="sector_around"></param>
void LockSectorAround(const SectorAround* sector_around);


/// <summary>
/// 주변 섹터 Lock 반환
/// </summary>
/// <param name="sector_around"></param>
void UnlockSectorAround(const SectorAround* sector_around);


void InitSector();
void ReleaseSector();
//void LockSector(const unsigned short y, const unsigned short x);
//void UnlockSector(const unsigned short y, const unsigned short x);


extern list<User*> g_SectorList[SECTOR_MAX_Y][SECTOR_MAX_X];
extern SRWLOCK g_SectorLock[SECTOR_MAX_Y][SECTOR_MAX_X];
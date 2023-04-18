#pragma once
using std::list;

#define SECTOR_MAX_X 50
#define SECTOR_MAX_Y 50


#define REMEMBER_ITER

enum SECTOR
{
	enSECTORADD = 0,		//0 : �߰�
	enSECTORDEL,			//1 : ����
	enSECTORLOCKSHARED,		//2 : �б� ��
	enSECTORUNLOCKSHARED,	//3 : �б� ���
	enSECTORLOCKEXCLUSIVE,	//4 : ���� ��
	enSECTORUNLOCKEXCLUSIVE	//5 : ���� ���
};


struct SectorPos
{
	SectorPos() {}
	SectorPos(DWORD _x, DWORD _y) : x(_x), y(_y)
	{

	}

	DWORD x;
	DWORD y;
};

struct SectorAround
{
	int count;
	SectorPos around[9];

};

/// <summary>
/// User �� ����� ���� ��ġ ����� �´� Sector List�� User �߰�
/// 
/// </summary>
/// <param name="user"></param>
void Sector_AddUser(User* user);

/// <summary>
/// ���� user�� ��ġ�� Sector���� user ����
/// 
/// </summary>
/// <param name="user"></param>
void Sector_RemoveUser(User* user);

/// <summary>
/// 
/// </summary>
/// <param name="sector_x"> �ֺ� ���� ���� Sector�� x ��ǥ </param>
/// <param name="sector_y"> �ֺ� ���� ���� Sector�� y ��ǥ </param>
/// <param name="sector_around"> [Out] �ֺ� ���� ���� </param>
void GetSectorAround(int sector_x, int sector_y, SectorAround* sector_around);

/// <summary>
/// �ֺ� ���Ϳ� ���� Lock ȹ��
/// </summary>
/// <param name="sector_around"></param>
void LockSectorAround(const SectorAround* sector_around);


/// <summary>
/// �ֺ� ���� Lock ��ȯ
/// </summary>
/// <param name="sector_around"></param>
void UnlockSectorAround(const SectorAround* sector_around);


void InitSector();
void ReleaseSector();
//void LockSector(const unsigned short y, const unsigned short x);
//void UnlockSector(const unsigned short y, const unsigned short x);


extern list<User*> g_SectorList[SECTOR_MAX_Y][SECTOR_MAX_X];
extern SRWLOCK g_SectorLock[SECTOR_MAX_Y][SECTOR_MAX_X];
//extern Tracer g_SecTracer;
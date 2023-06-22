
#include "User.h"

#include <Windows.h>

#include <cassert>



User::User()
{
	mb_login = false;
	mb_inSector = false;
	m_accountNo = -1;
	m_ID.reserve(MAX_ID_SIZE);
	m_nickname.reserve(MAX_NICK_SIZE);
	m_sessionKey.reserve(MAX_SESSION_KEY_SIZE);
	InitializeCriticalSection(&m_CS);

}

User::~User()
{

	DeleteCriticalSection(&m_CS);

}

void User::Lock()
{
	EnterCriticalSection(&m_CS);
	return;
}

void User::Unlock()
{
	LeaveCriticalSection(&m_CS);
	return;
}

void User::MoveSector(short sectorX, short sectorY)
{
	assert(sectorX >= 0);
	assert(sectorY >= 0);

	m_sectorX = sectorX; 
	m_sectorY = sectorY;
}
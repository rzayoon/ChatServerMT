#pragma once
#include "Define.h"

#include <Windows.h>

#include <list>
#include <string>




class User
{


public:
	User();
	virtual ~User();

	unsigned long long GetLastRecvTime() const
	{
		return m_lastRecvTime;
	}

	void Lock();
	void Unlock();

	void InitUser(SS_ID SSID)
	{
		SetSSID(SSID);
		m_sectorX = -1;
		m_sectorY = -1;
		UpdateRecvTime();
		ResetLogin();
		ResetInSector();
	}


	void SetLogin() { mb_login = true; }
	void ResetLogin() { mb_login = false; }
	bool IsLogin() { return mb_login; } const 
	void SetInSector() { mb_inSector = true; }
	void ResetInSector() { mb_inSector = false; }
	bool IsInSector() { return mb_inSector; } const

	void SetSSID(SS_ID SSID) { m_SSID = SSID; }
	SS_ID GetSSID() { return m_SSID; } const

	__int64 GetAccountNo() { return m_accountNo; } const
	void SetAccountNo(__int64 accountNo) { m_accountNo = accountNo; }

	void MoveSector(short sectorX, short sectorY);
	void GetSector(short* const sectorX, short* const sectorY) const
	{
		*sectorX = m_sectorX; *sectorY = m_sectorY;
	}

	void SetSectorIter(const std::list<User*>::iterator& iter)
	{
		m_sectorIter = iter;
	}

	short GetSectorX() { return m_sectorX; } const
	short GetSectorY() { return m_sectorY; } const

	std::list<User*>::iterator GetSectorIter() const
	{
		return m_sectorIter;
	}

	void UpdateRecvTime()
	{
		m_oldRecvTime = m_lastRecvTime;
		m_lastRecvTime = GetTickCount64();
	}

	void SetIDByWCS(const wchar_t* const id)
	{
		m_ID = id;
	}

	const std::wstring& GetID() const
	{
		return m_ID;
	}

	void SetNicknameByWCS(const wchar_t* const nick)
	{
		m_nickname = nick;
	}

	const std::wstring& GetNickname() const
	{
		return m_nickname;
	}

	void SetSessionKeyByWCS(const wchar_t* const sessionKey)
	{
		m_sessionKey = sessionKey;
	}


private:
	bool mb_login;
	bool mb_inSector;

	SS_ID m_SSID;
	__int64 m_accountNo;
	std::wstring m_ID;
	std::wstring m_nickname;
	std::wstring m_sessionKey;

	short m_sectorX;
	short m_sectorY;
	
	std::list<User*>::iterator m_sectorIter;

	unsigned long long m_lastRecvTime;
	unsigned long long m_oldRecvTime;

	CRITICAL_SECTION m_CS;

};

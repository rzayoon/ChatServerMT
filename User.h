#pragma once
#include <Windows.h>

typedef unsigned long long SS_ID;

#define MAX_ID_SIZE 20
#define MAX_NICK_SIZE 20
#define MAX_SESSION_KEY_SIZE 64


class User
{
public:
	User();
	virtual ~User();

	unsigned long long GetLastRecvTime()
	{
		return last_recv_time;
	}

	void Lock()
	{
		EnterCriticalSection(&cs);
		return;
	}

	void Unlock()
	{
		LeaveCriticalSection(&cs);
		return;
	}


	bool is_login;
	bool is_in_sector;

	SS_ID session_id;
	__int64 account_no;
	wchar_t id[MAX_ID_SIZE];
	wchar_t nickname[MAX_NICK_SIZE];
	char session_key[MAX_SESSION_KEY_SIZE];

	unsigned short sector_x;
	unsigned short sector_y;

	unsigned long long last_recv_time;

	CRITICAL_SECTION cs;



};


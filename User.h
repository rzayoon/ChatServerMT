#pragma once
#include <Windows.h>

using SS_ID = unsigned long long;


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

	short sector_x;
	short sector_y;
	
	list<User*>::iterator sector_iter;

	unsigned long long last_recv_time;
	unsigned long long old_recv_time;

	CRITICAL_SECTION cs;



};


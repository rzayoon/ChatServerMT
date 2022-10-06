#include "User.h"


User::User()
{
	is_login = false;
	is_in_sector = false;
	account_no = -1;

	InitializeCriticalSection(&cs);

}

User::~User()
{

	DeleteCriticalSection(&cs);

}




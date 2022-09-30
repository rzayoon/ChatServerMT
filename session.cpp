#include "session.h"




Session::Session()
{

	sock = 0;
	port = 0;
	session_id = 0;
	io_count = 0;
	send_flag = false;
	send_packet_cnt = 0;
	InitializeCriticalSection(&session_cs);
}

Session::~Session()
{
	DeleteCriticalSection(&session_cs);

}

void Session::Lock()
{
	EnterCriticalSection(&session_cs);
	return;
}

void Session::Unlock()
{
	LeaveCriticalSection(&session_cs);
}
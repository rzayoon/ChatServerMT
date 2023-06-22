#include <Windows.h>

#include <stdlib.h>
#include <stdio.h>

#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "MemoryPoolTls.h"

#include "CPacket.h"

#include "RingBuffer.h"
#include "Tracer.h"

#include "session.h"




Session::Session()
{

	sock = 0;
	port = 0;
	session_id = 0;
	io_count = 0;
	send_post_flag = 0;
	
	release_flag = true;
	disconnect = true;
}

Session::~Session()
{

}



unsigned long long Session::GetSessionID()
{
	return *((unsigned long long*)&session_id);
}
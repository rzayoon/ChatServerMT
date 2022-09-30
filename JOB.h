#pragma once
#include "CPacket.h"
#include "User.h"


#define MAX_JOB_QUEUE 20000

struct JOB
{
	enum
	{

		ON_RECV = 1,
		ON_CLI_JOIN,
		ON_CLI_LEAVE
	};

	unsigned char type;
	SS_ID id;

	CPacket* packet;
};
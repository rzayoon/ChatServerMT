#pragma once
#include "User.h"
#include "CPacket.h"

class PacketProcessor
{
public:
	static bool ProcLogin(User* user, CPacket* packet);
	static bool ProcSectorMove(User* user, CPacket* packet);
	static bool ProcMessage(User* user, CPacket* packet);

};


#pragma once
#include "User.h"
#include "CPacket.h"



bool ProcChatLogin(User* user, CPacket* packet);
bool ProcChatSectorMove(User* user, CPacket* packet);
bool ProcChatMessage(User* user, CPacket* packet);



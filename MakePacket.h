#pragma once
#include "CPacket.h"


void MakeChatLogin(CPacket* packet, BYTE status, __int64 account_no);

void MakeChatSectorMove(CPacket* packet, __int64 account_no, WORD sector_x, WORD sector_y);

void MakeChatMessage(CPacket* packet, __int64 account_no, WCHAR* id, WCHAR* nickname,
	WORD message_len, WCHAR* message);
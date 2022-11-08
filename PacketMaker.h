#pragma once
#include "CPacket.h"

class PacketMaker
{
public:

	static void MakeLogin(CPacket* packet, BYTE status, __int64 account_no);

	static void MakeSectorMove(CPacket* packet, __int64 account_no, WORD sector_x, WORD sector_y);

	static void MakeMessage(CPacket* packet, __int64 account_no, WCHAR* id, WCHAR* nickname,
		WORD message_len, WCHAR* message);

};


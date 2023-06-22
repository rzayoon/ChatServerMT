#pragma once

#include <string>
using std::wstring;

class PacketMaker
{
public:

	static void MakeLogin(CPacket* const packet, const BYTE status, const __int64 account_no);

	static void MakeSectorMove(CPacket* const packet, const __int64 account_no, const WORD sector_x, const WORD sector_y);

	static void MakeMessage(CPacket* const packet, __int64 account_no, const wstring& id, const wstring& nickname,
		const WORD message_len, const wstring& message);

};


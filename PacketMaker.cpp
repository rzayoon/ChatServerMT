#include <Windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <list>
using std::wstring;
using std::list;

#include "LockFreeQueue.h"
#include "LockFreeStack.h"
#include "MemoryPoolTls.h"

#include "CPacket.h"

#include "PacketMaker.h"
#include "User.h"
#include "CommonProtocol.h"

void PacketMaker::MakeLogin(CPacket* packet, BYTE status, __int64 account_no)
{
	WORD type = en_PACKET_CS_CHAT_RES_LOGIN;
	(*packet) << type << status << account_no;

	(*packet).Encode();

	return;
}

void PacketMaker::MakeSectorMove(CPacket* packet, __int64 account_no, WORD sector_x, WORD sector_y)
{
	WORD type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	(*packet) << type << account_no << sector_x << sector_y;

	(*packet).Encode();

	return;
}


void PacketMaker::MakeMessage(CPacket* const packet, __int64 account_no, const wstring& id, const wstring& nickname,
	const WORD message_len, const wstring& message)
{
	unsigned __int16 type = en_PACKET_CS_CHAT_RES_MESSAGE;

	(*packet) << type << account_no;

	wchar_t temp_id[MAX_ID_SIZE];
	wcscpy_s(temp_id, id.c_str());

	wchar_t temp_nickname[MAX_NICK_SIZE];
	wcscpy_s(temp_nickname, nickname.c_str());

	(*packet).PutData((char*)temp_id, MAX_ID_SIZE * sizeof(wchar_t));
	(*packet).PutData((char*)temp_nickname, MAX_NICK_SIZE * sizeof(wchar_t));

	(*packet) << message_len;
	(*packet).PutData((char*)message.c_str(), message_len);

	(*packet).Encode();

	return;
}
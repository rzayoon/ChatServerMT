#include "MakePacket.h"
#include "User.h"
#include "CommonProtocol.h"

void MakeChatLogin(CPacket* packet, BYTE status, __int64 account_no)
{
	WORD type = en_PACKET_CS_CHAT_RES_LOGIN;
	(*packet) << type << status << account_no;

	(*packet).Encode();

	return;
}

void MakeChatSectorMove(CPacket* packet, __int64 account_no, WORD sector_x, WORD sector_y)
{
	WORD type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	(*packet) << type << account_no << sector_x << sector_y;

	(*packet).Encode();

	return;
}


void MakeChatMessage(CPacket* packet, __int64 account_no, WCHAR* id, WCHAR* nickname,
	WORD message_len, WCHAR* message)
{
	WORD type = en_PACKET_CS_CHAT_RES_MESSAGE;

	(*packet) << type << account_no;

	


	(*packet).PutData((char*)id, MAX_ID_SIZE * sizeof(WCHAR));
	(*packet).PutData((char*)nickname, MAX_NICK_SIZE * sizeof(WCHAR));

	(*packet) << message_len;
	(*packet).PutData((char*)message, message_len);

	(*packet).Encode();

	return;

}
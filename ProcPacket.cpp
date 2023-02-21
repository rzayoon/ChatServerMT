

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#include <cpp_redis/cpp_redis>

#include <unordered_map>
using std::unordered_map;
#include <vector>
using std::vector;
#include <algorithm>
#include <string>

#include <Windows.h>
#include "CPacket.h"
#include "Tracer.h"
#include "CrashDump.h"

#include "MemoryPoolTls.h"

#include "RingBuffer.h"
#include "CLanClient.h"
#include "MonitorClient.h"
#include "CCpuUsage.h"
#include "CPDH.h"

#include "session.h"
#include "CNetServer.h"

#include "User.h"
#include "ChatServer.h"
#include "ProcPacket.h"
#include "PacketMaker.h"
#include "Sector.h"
#include "CommonProtocol.h"
#include "ProfileTls.h"

#include "CLog.h"

extern ChatServer g_chatServer;




bool PacketProcessor::ProcLogin(User* user, CPacket* packet)
{
	__int64 account_no;
	wchar_t id[MAX_ID_SIZE];
	wchar_t nick[MAX_NICK_SIZE];
	int iCnt;

	Profile pro = Profile(L"Login");

	int idx = user->session_id & dfUSER_MAP_HASH;
	bool ret = true;

	(*packet) >> account_no;
	(*packet).GetData((char*)id, MAX_ID_SIZE * sizeof(wchar_t));
	(*packet).GetData((char*)nick, MAX_NICK_SIZE * sizeof(wchar_t));
	
	char session_key[65];
	(*packet).GetData((char*)session_key, 64);
	session_key[64] = '\0';

	if (user->is_login == true)
	{
		// 같은 세션id에서 중복 로그인 메시지

		CPacket* send_packet = CPacket::Alloc();
		
		PacketMaker::MakeLogin(send_packet, 0, account_no);

		g_chatServer.SendMessageUni(send_packet, user);

		CPacket::Free(send_packet);
		
		ret = false;
	}
	if (!ret) return false;

	{
		AcquireSRWLockExclusive(&g_chatServer.m_accountSRW);
		auto iter = g_chatServer.m_accountMap.find(account_no);
		if (iter != g_chatServer.m_accountMap.end())
		{
			InterlockedIncrement(&g_chatServer.m_duplicateLogin);
			Log(L"SYS", enLOG_LEVEL_ERROR, L"Duplicated Login : account no[%d]", account_no);
			g_chatServer.DisconnectSession(iter->second);
			ret = false;
		}
		else
		{
			g_chatServer.m_accountMap[account_no] = user->session_id;
			ret = true;
		}
		ReleaseSRWLockExclusive(&g_chatServer.m_accountSRW);

	}
	if (!ret) return false;

	// session key redis 에서 확인
	BYTE status;
#ifdef dfCONREDIS
	auto get = g_chatServer.m_redis.get(std::to_string(account_no));
	g_chatServer.m_redis.sync_commit();


	user->account_no = account_no;

	if (strcmp(session_key, get.get().as_string().c_str()) == 0)
	{
		status = 1;
		InterlockedDecrement(&g_chatServer.m_connectCnt);
		InterlockedIncrement(&g_chatServer.m_loginCnt);

		user->is_login = true;

		wcscpy_s(user->id, id);
		wcscpy_s(user->nickname, nick);
		ret = true;
	}
	else
	{
		status = 0;
		ret = false;
		Log(L"Login", enLOG_LEVEL_DEBUG, L"Session Key Error account %lld", account_no);

	}
#else
	status = 1;
	InterlockedDecrement(&g_chatServer.m_connectCnt);
	InterlockedIncrement(&g_chatServer.m_loginCnt);

	user->is_login = true;
	ret = true;

#endif


	CPacket* send_packet = CPacket::Alloc();
	PacketMaker::MakeLogin(send_packet, status, user->account_no);
	g_chatServer.SendMessageUni(send_packet, user);
	CPacket::Free(send_packet);

	return ret;
}

bool PacketProcessor::ProcSectorMove(User* user, CPacket* packet)
{
	__int64 account_no;
	short sector_x;
	short sector_y;
	Profile pro = Profile(L"SectorMove");

	(*packet) >> account_no >> sector_x >> sector_y;



	if (account_no != user->account_no)
	{
		// 오류

		return false;
	}


	if (sector_x < 0 || sector_x >= SECTOR_MAX_X || sector_y < 0 || sector_y >= SECTOR_MAX_Y)
	{
		return false;
	}


	vector<SectorPos> lock_sector;
	lock_sector.reserve(2);
	
	lock_sector.push_back({ static_cast<DWORD>(sector_x), static_cast<DWORD>(sector_y) });
	if (user->is_in_sector && (sector_x != user->sector_x || sector_y != user->sector_y))
	{
		lock_sector.push_back({ static_cast<DWORD>(user->sector_x), static_cast<DWORD>(user->sector_y) });
	}


	std::sort(lock_sector.begin(), lock_sector.end(),
		[](SectorPos a, SectorPos b)
		{
			if (a.y < b.y) return true;
			else if (a.y == b.y && a.x < b.x) return true;

			return false;
		});

	for (int i = 0; i < lock_sector.size(); i++)
	{
		AcquireSRWLockExclusive(&g_SectorLock[lock_sector[i].y][lock_sector[i].x]);
	}

	
	Sector_RemoveUser(user);

	user->sector_x = sector_x;
	user->sector_y = sector_y;

	Sector_AddUser(user);

	for (int i = 0; i < lock_sector.size(); i++)
	{
		ReleaseSRWLockExclusive(&g_SectorLock[lock_sector[i].y][lock_sector[i].x]);
	}

	CPacket* send_packet = CPacket::Alloc();

	PacketMaker::MakeSectorMove(send_packet, user->account_no, user->sector_x, user->sector_y);

	g_chatServer.SendMessageUni(send_packet, user);

	CPacket::Free(send_packet);

	return true;

}

bool PacketProcessor::ProcMessage(User* user, CPacket* packet)
{
	__int64 account_no;
	WORD message_len;

	wchar_t message[MAX_MESSAGE];
	Profile pro = Profile(L"ChatMessage");


	(*packet) >> account_no >> message_len;

	if (user->account_no != account_no)
		return false;

	if ((*packet).GetDataSize() != message_len || message_len == 0)
		return false;

	(*packet).GetData((char*)message, message_len);


	CPacket* send_packet = CPacket::Alloc();

	PacketMaker::MakeMessage(send_packet, account_no, user->id, user->nickname, message_len, message);

	g_chatServer.SendMessageAround(send_packet, user);

	CPacket::Free(send_packet);

	return true;
}
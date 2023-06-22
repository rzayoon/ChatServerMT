
#include <Windows.h>

#include <unordered_map>
using std::unordered_map;
#include <vector>
using std::vector;
#include <algorithm>
#include <list>
using std::list;
#include <stdlib.h>
#include <stdio.h>


#include "CrashDump.h"

#include "LockFreeQueue.h"
#include "LockFreeStack.h"
#include "MemoryPoolTls.h"

#include "CPacket.h"
#include "RingBuffer.h"
#include "Tracer.h"
#include "session.h"


#include "User.h"

#include "MonitorClient.h"
#include "CCpuUsage.h"
#include "CPDH.h"

#include "ChatServer.h"
#include "ProcPacket.h"
#include "PacketMaker.h"
#include "Sector.h"
#include "CommonProtocol.h"
#include "ProfileTls.h"

#include "CLog.h"

bool PacketProcessor::ProcLogin(User* user, CPacket* packet)
{
	__int64 account_no;
	wchar_t id[MAX_ID_SIZE];
	wchar_t nick[MAX_NICK_SIZE];
	int iCnt;

	Profile pro = Profile(L"Login");

	SS_ID sessionID = user->GetSSID();
	int idx = sessionID & dfUSER_MAP_HASH;
	bool ret = true;

	(*packet) >> account_no;
	(*packet).GetData((char*)id, MAX_ID_SIZE * sizeof(wchar_t));
	(*packet).GetData((char*)nick, MAX_NICK_SIZE * sizeof(wchar_t));
	// null terminator check..?


	char session_key[64];

	(*packet).GetData((char*)session_key, 64); // 아직 안씀

	if (user->IsLogin())
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
		AcquireSRWLockExclusive(&g_chatServer.m_accountMapSRW);
		auto iter = g_chatServer.m_accountMap.find(account_no);

		if (iter != g_chatServer.m_accountMap.end())
		{
			// 중복
			InterlockedIncrement(&g_chatServer.m_duplicateLogin);
			CrashDump::Crash();
			// 임계영역에서 로그 남기는거 지양해야함
			Log(L"Chat", enLOG_LEVEL_ERROR, L"Duplicated Login [%lld]", account_no);
			g_chatServer.DisconnectSession(iter->second);
			ret = false;
		}
		else
		{
			g_chatServer.m_accountMap[account_no] = sessionID;
			ret = true;
		}
		ReleaseSRWLockExclusive(&g_chatServer.m_accountMapSRW);


		if (!ret) return false;

	}
	InterlockedDecrement(&g_chatServer.m_connectCnt);
	InterlockedIncrement(&g_chatServer.m_loginCnt);

	user->SetLogin();
	user->SetAccountNo(account_no);
	user->SetIDByWCS(id);
	user->SetNicknameByWCS(nick);


	CPacket* send_packet = CPacket::Alloc();

	PacketMaker::MakeLogin(send_packet, 1, account_no);
	g_chatServer.SendMessageUni(send_packet, user);

	CPacket::Free(send_packet);

	return true;
}

bool PacketProcessor::ProcSectorMove(User* user, CPacket* packet)
{
	__int64 account_no;
	short sector_x;
	short sector_y;
	Profile pro = Profile(L"SectorMove");

	(*packet) >> account_no >> sector_x >> sector_y;



	if (account_no != user->GetAccountNo())
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

	// 이동할 곳
	lock_sector.push_back({ static_cast<DWORD>(sector_x), static_cast<DWORD>(sector_y) });

	short userSectorX = user->GetSectorX();
	short userSectorY = user->GetSectorY();

	// 현재 위치. 로그인 후 첫 이동이면 skip
	if (user->IsInSector() && (sector_x != userSectorX || sector_y != userSectorY))
	{
		lock_sector.push_back({ static_cast<DWORD>(userSectorX), static_cast<DWORD>(userSectorY) });
	}

	// 잠금 우선 순위 반영
	std::sort(lock_sector.begin(), lock_sector.end(),
		[](SectorPos a, SectorPos b)
		{
			if (a.y < b.y) return true;
			else if (a.y == b.y && a.x < b.x) return true;

			return false;
		});

	for (int i = 0; i < lock_sector.size(); i++)
	{
		DWORD y = lock_sector[i].y;
		DWORD x = lock_sector[i].x;

		AcquireSRWLockExclusive(&g_SectorLock[y][x]);
	}

	Sector_RemoveUser(user);

	user->MoveSector(sector_x, sector_y);

	Sector_AddUser(user);

	for (int i = 0; i < lock_sector.size(); i++)
	{
		DWORD y = lock_sector[i].y;
		DWORD x = lock_sector[i].x;

		ReleaseSRWLockExclusive(&g_SectorLock[y][x]);
	}

	CPacket* send_packet = CPacket::Alloc();

	PacketMaker::MakeSectorMove(send_packet, account_no, user->GetSectorX(), user->GetSectorY());

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

	if (user->GetAccountNo() != account_no)
		return false;

	if ((*packet).GetDataSize() != message_len || message_len == 0 || message_len > MAX_MESSAGE)
		return false;

	(*packet).GetData((char*)message, message_len);
	message[message_len / 2] = L'\0';
	wstring ws_message = message;

	CPacket* send_packet = CPacket::Alloc();

	PacketMaker::MakeMessage(send_packet, account_no, user->GetID(), user->GetNickname(), message_len, ws_message);

	g_chatServer.SendMessageAround(send_packet, user);

	CPacket::Free(send_packet);

	return true;
	

}
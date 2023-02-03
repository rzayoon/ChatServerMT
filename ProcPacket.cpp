

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#include <cpp_redis/cpp_redis>

#include <unordered_map>
using std::unordered_map;
#include <string>

#include <Windows.h>
#include "CPacket.h"
#include "Tracer.h"
#include "CrashDump.h"

#include "User.h"
#include "MemoryPoolTls.h"

#include "RingBuffer.h"
#include "CLanClient.h"
#include "MonitorClient.h"
#include "CCpuUsage.h"
#include "CPDH.h"

#include "session.h"
#include "CNetServer.h"
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
		//for (iCnt = 0; iCnt < dfUSER_MAP_HASH; iCnt++)
		//{
		//	AcquireSRWLockShared(&g_chatServer.m_userMapCS[iCnt]);
		//}

		//// 남아있는 user account 확인

		//for (iCnt = 0; iCnt < dfUSER_MAP_HASH; iCnt++)
		//{
		//	for (auto& iter : g_chatServer.m_userMap[iCnt])
		//	{
		//		ULONGLONG time = GetTickCount64();
		//		User* temp_user = iter.second;
		//		if (temp_user->account_no == account_no)
		//		{
		//			// 다른 세션 id에서 account no 중복 로그인

		//			ULONGLONG t = GetTickCount64();

		//			/*CPacket* send_packet = CPacket::Alloc();

		//			MakeChatLogin(send_packet, 0, account_no);

		//			SendMessageUni(send_packet, user);

		//			CPacket::Free(send_packet);*/

		//			InterlockedIncrement(&g_chatServer.m_duplicateLogin);
		//			Log(L"SYS", enLOG_LEVEL_ERROR, L"Duplicated Login : session id[%lld] account no[%lld]", temp_user->session_id, temp_user->account_no);
		//			//g_chatServer.DisconnectSession(temp_user->session_id);

		//			ret = true; // 안끊는 상태로 테스트
		//		}
		//	}
		//	if (!ret) break;
		//}
		//for (iCnt = 0; iCnt < dfUSER_MAP_HASH; iCnt++)
		//{
		//	ReleaseSRWLockShared(&g_chatServer.m_userMapCS[iCnt]);
		//}

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
	auto get = g_chatServer.m_redis.get(std::to_string(account_no));
	g_chatServer.m_redis.sync_commit();

	BYTE status;
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

	

	// todo SectorMove 함수화 하기
	short lock_y[2];
	short lock_x[2];
	int lock_cnt = 1;

	if (!user->is_in_sector) // 처음
	{
		lock_y[0] = sector_y;
		lock_x[0] = sector_x;
	}
	else
	{
		
		if (user->sector_y < sector_y)
		{
			lock_y[0] = user->sector_y;
			lock_x[0] = user->sector_x;
			lock_y[1] = sector_y;
			lock_x[1] = sector_x;

			lock_cnt++;
		}
		else if (user->sector_y > sector_y)
		{
			lock_y[0] = sector_y;
			lock_x[0] = sector_x;
			lock_y[1] = user->sector_y;
			lock_x[1] = user->sector_x;

			lock_cnt++;
		}
		else // y 같음
		{
			if (user->sector_x < sector_x)
			{
				lock_y[0] = user->sector_y;
				lock_x[0] = user->sector_x;
				lock_y[1] = sector_y;
				lock_x[1] = sector_x;

				lock_cnt++;
			}
			else if(user->sector_x > sector_x)
			{
				lock_y[0] = sector_y;
				lock_x[0] = sector_x;
				lock_y[1] = user->sector_y;
				lock_x[1] = user->sector_x;

				lock_cnt++;
			}
			else
			{
				lock_y[0] = sector_y;
				lock_x[0] = sector_x;

			}
		}
	}

	for (int i = 0; i < lock_cnt; i++)
	{
		AcquireSRWLockExclusive(&g_SectorLock[lock_y[i]][lock_x[i]]);
	}

	if(user->is_in_sector)
		Sector_RemoveUser(user);

	user->sector_x = sector_x;
	user->sector_y = sector_y;

	Sector_AddUser(user);

	for (int i = 0; i < lock_cnt; i++)
	{
		ReleaseSRWLockExclusive(&g_SectorLock[lock_y[i]][lock_x[i]]);
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
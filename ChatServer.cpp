#include <thread>
#include <utility>
#include <unordered_map>

#include "MemoryPoolTls.h"
#include "ChatServer.h"
#include "ChatLogic.h"
#include "CommonProtocol.h"
#include "ProcPacket.h"
#include "Sector.h"
#include "ObjectPool.h"
#include "CrashDump.h"

ChatServer g_server;



// session key -> account no 중복 검사 시 순회 필요 ( login )
// account no key -> ?? session key 중복 검사?? join - leave 시 검색해서 찾아야 함.



ChatServer::ChatServer()
{
	//m_hSingleThread = (HANDLE)_beginthreadex(nullptr, 0, SingleUpdate, nullptr, 0, nullptr);
	InitializeSRWLock(&g_UserMapSRW);

	InitSector();

}


ChatServer::~ChatServer()
{
	ReleaseSector();

}

bool ChatServer::OnConnectionRequest(wchar_t* ip, unsigned short port)
{
	if (g_login_cnt > max_user) // 느슨하게
	{
		return false;
	}

	return true;
}

void ChatServer::OnClientJoin(unsigned long long session_id)
{
	CreateUser(session_id);

	return;
}

void ChatServer::OnClientLeave(unsigned long long session_id)
{
	
	DeleteUser(session_id);

	return;
}

void ChatServer::OnRecv(unsigned long long session_id, CPacket* packet)
{
	WORD packet_type;
	(*packet) >> packet_type;
	User* user;

	DWORD t = GetTickCount64();

	if (!AcquireUser(session_id, &user))
		CrashDump::Crash();

	unsigned int acc = user->account_no;

	user->last_recv_time = GetTickCount64();

	bool result_proc = false;

	// Packet Proc
	switch (packet_type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
	{
		g_Tracer.trace(10, (PVOID)user->session_id);
		result_proc = ProcChatLogin(user, packet);
		break;
	}
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
	{
		g_Tracer.trace(11, (PVOID)user->session_id);
		result_proc = ProcChatSectorMove(user, packet);
		break;
	}
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
	{
		g_Tracer.trace(12, (PVOID)user->session_id);
		result_proc = ProcChatMessage(user, packet);
		break;
	}
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
	{
		//  필요 없음
		break;
	}
	default: // undefined protocol
	{
		// 예외 처리


	}

	}

	if (!result_proc) 
		g_server.DisconnectSession(user->session_id);

	ReleaseUser(user);

	return;
}



void ChatServer::OnSend(unsigned long long session_id, int send_size)
{

	return;
}


void ChatServer::OnWorkerThreadBegin()
{
	return;
}

void ChatServer::OnWorkerThreadEnd()
{
	return;
}

void ChatServer::OnError(int errorcode, const wchar_t* msg)
{

	return;
}



#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#include <cpp_redis/cpp_redis>

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#include <stdio.h>
#include <conio.h>
#include <time.h>

#include <unordered_map>
using std::unordered_map;

#include "CPacket.h"
#include "CrashDump.h"
#include "MemoryPoolTls.h"

#include "User.h"

#include "Tracer.h"
#include "RingBuffer.h"
#include "CLanClient.h"
#include "MonitorClient.h"
#include "CCpuUsage.h"
#include "CPDH.h"

#include "session.h"
#include "CNetServer.h"
#include "ChatServer.h"
#include "TextParser.h"
#include "ProfileTls.h"
#include "CLog.h"

#define SERVERPORT 6000



int main()
{

	timeBeginPeriod(1);
	
	char ip[16];
	int port;
	int worker;
	int max_worker;
	int max_user;
	int max_session;
	int packet_code;
	int packet_key;
	int nagle;
	int monitor_reconnect;
	SYSLOG_Init(L"Log", enLOG_LEVEL_DEBUG);

	TextParser parser;
	if (!parser.LoadFile("Config.ini")) return 1;
	wchar_t wip[16];

	parser.GetStringValue("RedisIP", ip, 16);
	parser.GetValue("RedisPort", &port);
	g_chatServer.SetRedisInfo(ip, port);
	g_chatServer.ConnectRedis();

	parser.GetStringValue("ServerBindIP", ip, 16);
	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);
	parser.GetValue("ServerBindPort", &port);
	parser.GetValue("IOCPWorkerThread", &worker);
	parser.GetValue("IOCPActiveThread", &max_worker);
	parser.GetValue("MaxUser", &max_user);
	parser.GetValue("MaxSession", &max_session);
	parser.GetValue("PacketCode", &packet_code);
	parser.GetValue("PacketKey", &packet_key);
	parser.GetValue("AutoReconnectMonitor", &monitor_reconnect);


	parser.GetValue("Nagle", &nagle);
	g_chatServer.Start(wip, port, worker, max_worker, max_session, nagle, packet_key, packet_code);
	
	parser.GetStringValue("MonitorIP", ip, 16);
	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);
	parser.GetValue("MonitorPort", &port);

	g_chatServer.SetMonitorClientInfo(wip, port);

	
	g_chatServer.ConnectMonitor();



	DWORD oldTick = timeGetTime();
	while (1)
	{
		if (_kbhit())
		{
			wchar_t input;
			input = _getwch();

			if (input == L'q' || input == L'Q')
			{

				g_chatServer.Stop();

				break;
			}
			if (input == L'p' || input == L'P')
				ProfileDataOutText(L"profile.txt");
		}

		Sleep(1000);
		if(monitor_reconnect && !g_chatServer.IsConnectedMonitor())
			g_chatServer.ConnectMonitor();

		g_chatServer.Collect();
		if(g_chatServer.IsConnectedMonitor())
			g_chatServer.SendMonitor(time(NULL));
		g_chatServer.Show();
		
	}

	timeEndPeriod(1);

	wprintf(L"Fine Closing\n");
	Sleep(2000);


	return 0;
}

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#include <stdio.h>
#include <conio.h>
#include <time.h>

#include <unordered_map>
using std::unordered_map;

#include "CNetServer.h"
#include "User.h"
#include "MemoryPoolTls.h"

#include "ChatServer.h"
#include "TextParser.h"
#include "ProfileTls.h"
#include "CCpuUsage.h"
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
	SYSLOG_Init(L"Log", enLOG_LEVEL_DEBUG);

	TextParser parser;
	if (!parser.LoadFile("Config.ini")) return 1;
	wchar_t wip[16];

	parser.GetStringValue("ServerBindIP", ip, 16);
	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);
	parser.GetValue("ServerBindPort", &port);
	parser.GetValue("IOCPWorkerThread", &worker);
	parser.GetValue("IOCPActiveThread", &max_worker);
	parser.GetValue("MaxUser", &max_user);
	parser.GetValue("MaxSession", &max_session);
	parser.GetValue("PacketCode", &packet_code);
	parser.GetValue("PacketKey", &packet_key);

	parser.GetValue("Nagle", &nagle);

	g_chatServer.Start(wip, port, worker, max_worker, max_session, nagle, packet_key, packet_code);
	
	parser.GetStringValue("MonitorIP", ip, 16);
	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);
	parser.GetValue("MonitorPort", &port);
	parser.GetValue("ClientIOCPWorker", &worker);
	parser.GetValue("ClientIOCPActive", &max_worker);

	

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
		if(!g_chatServer.IsConnectedMonitor())
			g_chatServer.ConnectMonitor(wip, port, worker, max_worker, nagle);

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
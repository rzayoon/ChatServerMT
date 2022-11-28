
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#include <stdio.h>
#include <conio.h>

#include <unordered_map>
using std::unordered_map;


#include "User.h"
#include "MemoryPoolTls.h"

#include "ChatServer.h"
#include "MonitorClient.h"
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

	TextParser parser;
	if (!parser.LoadFile("Config.ini")) return 1;

	parser.GetStringValue("ChatBindIP ", ip, 16);
	parser.GetValue("ChatBindPort", &port);
	parser.GetValue("ChatIOCPWorker", &worker);
	parser.GetValue("ChatIOCPActive", &max_worker);
	parser.GetValue("MaxUser", &max_user);
	parser.GetValue("MaxSession", &max_session);
	parser.GetValue("ChatPacketCode", &packet_code);
	parser.GetValue("ChatPacketKey", &packet_key);

	parser.GetValue("Nagle", &nagle);

	wchar_t wip[16];

	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);

	SYSLOG_Init(L"Log", enLOG_LEVEL_DEBUG);

	g_chatServer.Start(wip, port, worker, max_worker, max_session, nagle, packet_key, packet_code);
	
	parser.GetStringValue("MonitorIP", ip, 16);
	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);
	parser.GetValue("MonitorPort", &port);
	parser.GetValue("MonitorIOCPWorker", &worker);
	parser.GetValue("MonitorIOCPActive", &max_worker);
	parser.GetValue("MonitorPacketCode", &packet_code);
	parser.GetValue("MonitorPacketKey", &packet_key);

	g_monitorCli.Connect(wip, port, worker, max_worker, nagle, packet_key, packet_code);

	CCpuUsage CpuTime;

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

		//Monitor Àü¼Û
		{
			g_chatServer.Show();
			CpuTime.Show();


		}
	}

	timeEndPeriod(1);

	wprintf(L"Fine Closing\n");
	Sleep(2000);


	return 0;
}
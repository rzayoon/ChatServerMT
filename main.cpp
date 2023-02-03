
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
	
	SYSLOG_Init(L"Log", enLOG_LEVEL_DEBUG);


	int monitor_reconnect;
	
	TextParser parser;
	if (!parser.LoadFile("Config.ini")) return 1;
	
	parser.GetValue("AutoReconnectMonitor", &monitor_reconnect);


	g_chatServer.Start();
	

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
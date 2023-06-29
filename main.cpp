
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
#include <list>
using std::list;
#include <unordered_set>
using std::unordered_set;

#include "RingBuffer.h"


#include "LockFreeQueue.h"
#include "LockFreeStack.h"

#include "MemoryPoolTls.h"
#include "CPacket.h"
#include "Tracer.h"

#include "session.h"

#include "CrashDump.h"

#include "CNetServer.h"
#include "User.h"
#include "CLanClient.h"

#include "MonitorClient.h"
#include "CCpuUsage.h"
#include "CPDH.h"

#include "ChatServer.h"

#include "TextParser.h"
#include "ProfileTls.h"
#include "CCpuUsage.h"
#include "CLog.h"

#define SERVERPORT 6000



int main()
{

	timeBeginPeriod(1);
	
	
	ChatServer* server = new ChatServer;

	server->Start();

	DWORD oldTick = timeGetTime();
	while (1)
	{
		if (_kbhit())
		{
			wchar_t input;
			input = _getwch();

			if (input == L'q' || input == L'Q')
			{

				server->Stop();

				break;
			}
			if (input == L'p' || input == L'P')
				ProfileDataOutText(L"profile.txt");
		}

		Sleep(1000);
		/*if(!g_chatServer.IsConnectedMonitor())
			g_chatServer.ConnectMonitor();*/

		server->Collect();
		/*if(g_chatServer.IsConnectedMonitor())
			g_chatServer.SendMonitor(time(NULL));*/
		server->Show();
		
	}

	timeEndPeriod(1);


	return 0;
}
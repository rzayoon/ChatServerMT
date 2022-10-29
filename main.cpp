
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#include <stdio.h>
#include <conio.h>

#include "ChatServer.h"
#include "ChatLogic.h"
#include "TextParser.h"
#include "ProfileTls.h"

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

	TextParser parser;
	if (!parser.LoadFile("Config.ini")) return 1;

	parser.GetStringValue("ServerBindIP", ip, 16);
	parser.GetValue("ServerBindPort", &port);
	parser.GetValue("IOCPWorkerThread", &worker);
	parser.GetValue("IOCPActiveThread", &max_worker);
	parser.GetValue("MaxUser", &max_user);
	parser.GetValue("MaxSession", &max_session);
	parser.GetValue("PacketCode", &packet_code);
	parser.GetValue("PacketKey", &packet_key);

	

	wchar_t wip[16];

	MultiByteToWideChar(CP_ACP, 0, ip, 16, wip, 16);

	g_server.Start(wip, port, worker, max_worker, max_session, max_user, packet_key, packet_code);

	DWORD oldTick = timeGetTime();
	while (1)
	{
		if (_kbhit())
		{
			wchar_t input;
			input = _getwch();

			if (input == L'q' || input == L'Q')
			{

				g_server.Stop();

				break;
			}
			if (input == L'p' || input == L'P')
				ProfileDataOutText(L"profile.txt");
		}

		Sleep(1000);

		unsigned int message_tps = InterlockedExchange(&g_message_tps, 0);

		g_server.Show();
		wprintf(L"Connect : %d\n"
			L"Login : %d\n"
			L"Duplicated login proc : %d\n"
			L"Message TPS : %d\n",
			g_connect_cnt, g_login_cnt, g_duplicate_login, message_tps);
	}

	timeEndPeriod(1);

	wprintf(L"Fine Closing\n");
	Sleep(5000);


	return 0;
}
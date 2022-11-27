#include "MonitorClient.h"
#include "CPDH.h"
#include "CLog.h"
#include "CommonProtocol.h"

MonitorClient g_monitorCli;


MonitorClient::MonitorClient()
{
	
}

MonitorClient::~MonitorClient()
{

}

void MonitorClient::OnEnterServerJoin()
{
	// 로그인 요청
	CPacket* sendPacket = MonitorPacket::Alloc();

	WORD type = en_PACKET_SS_MONITOR_LOGIN;
	int no = 3;
	*sendPacket << type << no;
	sendPacket->Encode();

	SendPacket(sendPacket);

	MonitorPacket::Free(sendPacket);

	return;
}

void MonitorClient::OnLeaveServer()
{
	

}

void MonitorClient::OnWorkerThreadBegin()
{
}

void MonitorClient::OnWorkerThreadEnd()
{
}

void MonitorClient::OnRecv(CPacket* packet)
{
	// 받을 거 없음.
}

void MonitorClient::OnError(int errorcode, const wchar_t* msg)
{
	Log(L"Monitor", enLOG_LEVEL_ERROR, L"[%d] %s", errorcode, msg);
}

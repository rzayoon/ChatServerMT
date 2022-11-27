#pragma once
#include "CPacket.h"


class MonitorPacket :
    public CPacket
{
public:

	inline static MemoryPoolTls<MonitorPacket> packet_pool = MemoryPoolTls<MonitorPacket>(500);
	inline static unsigned char packet_code = 0;
	inline static unsigned char packet_key = 0;

	inline static int GetUsePool()
	{
		return packet_pool.GetUseSize();
	}

	inline static void SetPacketCode(unsigned char code)
	{
		packet_code = code;
	}

	inline static void SetPacketKey(unsigned char key)
	{
		packet_key = key;
	}

	unsigned char GetPacketCode() {
		return packet_code;
	}
	unsigned char GetPacketKey()
	{
		return packet_key;
	}

	inline static MonitorPacket* Alloc()
	{
		MonitorPacket* packet = packet_pool.Alloc();
		packet->Clear();
		return packet;
	}

	inline static void Free(CPacket* packet)
	{
		packet->SubRef();
		return;
	}

	inline void SubRef()
	{
		int temp_cnt = InterlockedDecrement((LONG*)&ref_cnt);
		if (temp_cnt == 0)
			packet_pool.Free(this);
		return;
	}


};


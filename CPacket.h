#pragma once

#include <Windows.h>

#include "LockFreePool.h"
#include "MemoryPoolTls.h"

//#define AUTO_PACKET






class CPacket
{
	friend class CNetServer;
	friend class PacketPtr;
	friend class MemoryPoolTls<CPacket>;

public:
	enum
	{
		eBUFFER_DEFAULT = 2000
	};

private:	


	CPacket(int size = eBUFFER_DEFAULT);
	
	CPacket(CPacket& src);

	virtual ~CPacket();


	/// <summary>
	/// 패킷 파괴
	/// </summary>
	/// <param name=""></param>
	inline void Release(void);

public:

	/// <summary>
	/// 패킷 초기화
	/// </summary>
	/// <param name=""></param>
	inline void Clear(void);

	/// <summary>
	/// 버퍼 사이즈
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetBufferSize(void) { return buffer_size; }

	/// <summary>
	/// 사용중인 사이즈
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetDataSize(void) { return data_size; }

	/// <summary>
	/// 버퍼 포인터
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	char* GetBufferPtr(void) { return buffer; };

	char* GetReadPtr(void) { return buffer + read_pos; }

	/// <summary>
	/// 버퍼 쓰기 pos 이동
	/// 외부에서 직접 버퍼 내용 수정시 사용
	/// </summary>
	/// <param name="size">이동 사이즈</param>
	/// <returns>이동된 사이즈</returns>
	int MoveWritePos(int size);

	/// <summary>
	/// 버퍼 읽기 pos 이동
	/// 외부에서 직접 버퍼 내용 수정시 사용
	/// </summary>
	/// <param name="size">이동 사이즈</param>
	/// <returns>이동된 사이즈</returns>
	int MoveReadPos(int size);

	
	CPacket& operator=(CPacket& src);

	CPacket& operator<<(unsigned char value);
	CPacket& operator<<(char value);

	CPacket& operator<<(short value);
	CPacket& operator<<(unsigned short value);

	CPacket& operator<<(int value);
	CPacket& operator<<(unsigned int value);

	CPacket& operator<<(long value);
	CPacket& operator<<(unsigned long);

	CPacket& operator<<(float value);

	CPacket& operator<<(__int64 value);
	CPacket& operator<<(double value);

	CPacket& operator>>(unsigned char& value);
	CPacket& operator>>(char& value);


	CPacket& operator>>(short& value);
	CPacket& operator>>(unsigned short& value);

	CPacket& operator>>(int& value);
	CPacket& operator>>(unsigned int& value);
	CPacket& operator>>(long& value);
	CPacket& operator>>(unsigned long& value);
	CPacket& operator>>(float& value);

	CPacket& operator>>(__int64& value);
	CPacket& operator>>(double& value);

	int GetData(char* dest, int size);
	int PutData(char* src, int size);


	/// <summary>
	/// Packet을 할당 받는다. 
	/// 할당받은 Packet 객체는 Free(packet)으로 반환해야 함.
	/// 다른 스레드로 전달 시 Packet.AddRef()를 호출하고, 현재 스레드에서 Free(packet) 호출한다.
	/// 전달받은 스레드도 Free(packet)을 호출하여 정리하도록 한다.
	/// 
	/// </summary>
	/// <returns></returns>
	inline static CPacket* Alloc()
	{
		CPacket* packet = packet_pool.Alloc();
		packet->Clear();
		return packet;
	}

	/// <summary>
	/// Packet 반환
	/// 이 함수를 호출한 뒤에는 packet을 사용해서는 안된다.
	/// 레퍼런스 카운트가 감소되어 0일 경우 Packet Pool에 적재됨.
	/// </summary>
	/// <param name="packet"></param>
	inline static void Free(CPacket* packet)
	{
		packet->SubRef();

		return;
	}

	/// <summary>
	/// Packet을 다른 스레드로 넘길 경우 현재 스레드에서 정리하기 전에 호출한다.
	/// </summary>
	inline void AddRef()
	{
		InterlockedIncrement((LONG*)&ref_cnt);
		return;
	}

	/// <summary>
	/// subref 이후에는 바로 다른 스레드에서 사용할 여지가 있으므로 
	/// 건드리지 않는다.
	/// </summary>
	inline void SubRef()
	{
		int temp_cnt = InterlockedDecrement((LONG*)&ref_cnt);
		if (temp_cnt == 0)
			CPacket::packet_pool.Free(this);
		return;
	}

	/// <summary>
	/// Packet의 데이터 인코딩
	/// 하나의 패킷에 대해 전체 스레드에서 1회만 호출되어야 함.
	/// </summary>
	void Encode();
	/// <summary>
	/// 받은 패킷 디코딩
	/// </summary>
	/// <returns></returns>
	bool Decode();

private:
	char* GetBufferPtrLan(void);
	int GetDataSizeLan(void);
	char* GetBufferPtrNet(void);
	int GetDataSizeNet(void);


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

protected:

	// 정적 멤버
	inline static MemoryPoolTls<CPacket> packet_pool = MemoryPoolTls<CPacket>(1000);
	inline static unsigned char packet_code = 0;
	inline static unsigned char packet_key = 0;
	// 고정 
	char* buffer;
	char* hidden_buf;
	int buffer_size;
	
	// 변경 데이터 .. 다른 스레드에서 변하나..? 그렇지 않다.
	int data_size;
	int write_pos;
	int read_pos;
	alignas(64) int ref_cnt;

};


class PacketPtr
{

public:
	PacketPtr()
	{
		packet = nullptr;
	}

	PacketPtr(CPacket* new_packet)
	{
		packet = new_packet;
		packet->AddRef();
	}

	PacketPtr(PacketPtr& src)
	{
		packet = src.packet;
		if(packet)
			packet->AddRef();
	}


	~PacketPtr()
	{
		if (packet != nullptr)
		{
			packet->SubRef();
			packet = nullptr;
		}
	}

	PacketPtr& operator=(PacketPtr& src)
	{
		if (packet)
			packet->SubRef();
		packet = src.packet;
		if (packet)
			packet->AddRef();

		return *this;
	}

	CPacket* operator*()
	{
		return packet;
	}


private:

	CPacket* packet;

};
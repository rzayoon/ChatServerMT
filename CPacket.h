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


public:
	/// <summary>
	/// ��Ŷ �ı�
	/// </summary>
	/// <param name=""></param>
	inline void Release(void);

	/// <summary>
	/// ��Ŷ û��
	/// </summary>
	/// <param name=""></param>
	inline void Clear(void);

	/// <summary>
	/// ���� ������
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetBufferSize(void) { return buffer_size; }

	/// <summary>
	/// ������� ������
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetDataSize(void) { return data_size; }

	/// <summary>
	/// ���� ������
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	char* GetBufferPtr(void) { return buffer; };

	char* GetReadPtr(void) { return buffer + read_pos; }

	/// <summary>
	/// ���� ���� pos �̵�
	/// �ܺο��� ������ ���� ���� ������ ���
	/// </summary>
	/// <param name="size">�̵� ������</param>
	/// <returns>�̵��� ������</returns>
	int MoveWritePos(int size);

	/// <summary>
	/// ���� �б� pos �̵�
	/// �ܺο��� ������ ���� ���� ������ ���
	/// </summary>
	/// <param name="size">�̵� ������</param>
	/// <returns>�̵��� ������</returns>
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



	inline static CPacket* Alloc()
	{
		CPacket* packet = packet_pool.Alloc();
		packet->Clear();
		return packet;
	}

	inline static void Free(CPacket* packet)
	{
		packet->SubRef();

		return;
	}

	inline void AddRef()
	{
		InterlockedIncrement((LONG*)&ref_cnt);
		return;
	}

	/// <summary>
	/// subref ���Ŀ��� �ٷ� �ٸ� �����忡�� ����� ������ �����Ƿ� 
	/// �ǵ帮�� �ʴ´�.
	/// </summary>
	inline void SubRef()
	{
		int temp_cnt = InterlockedDecrement((LONG*)&ref_cnt);
		if (temp_cnt == 0)
			CPacket::packet_pool.Free(this);
		return;
	}

	void Encode();
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

	// ���� ���
	inline static MemoryPoolTls<CPacket> packet_pool = MemoryPoolTls<CPacket>(1000);
	inline static unsigned char packet_code = 0;
	inline static unsigned char packet_key = 0;
	// ���� 
	char* buffer;
	char* hidden_buf;
	int buffer_size;
	
	// ���� ������ .. �ٸ� �����忡�� ���ϳ�..? �׷��� �ʴ�.
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
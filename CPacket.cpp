#include <string.h>
#include "CPacket.h"
#include "NetProtocol.h"


CPacket::CPacket(int size)
{
	buffer_size = size;
	

	hidden_buf = new char[size + sizeof(NetPacketHeader)];
	buffer = hidden_buf + sizeof(NetPacketHeader);
	Clear();

}

CPacket::CPacket(CPacket& src)
	: CPacket(src.buffer_size)
{
	buffer_size = src.buffer_size;
	data_size = src.data_size;

	memcpy(buffer, src.buffer, src.buffer_size);
	write_pos = src.write_pos;
	read_pos = src.read_pos;
}

CPacket::~CPacket()
{
	Release();
}

void CPacket::Release(void)
{
	if (buffer != nullptr)	delete[] hidden_buf;
}

void CPacket::Clear(void)
{
	data_size = 0;
	write_pos = 0;
	read_pos = 0;
	ref_cnt = 1;

}

int CPacket::MoveWritePos(int size)
{
	if (size <= 0) return 0;

	if (write_pos + size >= buffer_size) size = buffer_size - write_pos - 1;

	write_pos += size;
	data_size += size;

	return size;
}

int CPacket::MoveReadPos(int size)
{
	if (size <= 0) return 0;

	if (read_pos + size > write_pos) size = write_pos - read_pos;

	read_pos += size;
	data_size -= size;

	return size;
}

CPacket& CPacket::operator=(CPacket& src)
{
	Release();
	buffer = new char[src.buffer_size];
	buffer_size = src.buffer_size;
	data_size = src.data_size;

	memcpy(buffer, src.buffer, buffer_size);
	write_pos = src.write_pos;
	read_pos = src.read_pos;

	return *this;
}

CPacket& CPacket::operator<<(unsigned char value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(char value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(short value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(unsigned short value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(int value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(unsigned int value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(long value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(unsigned long value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(float value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(__int64 value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator<<(double value)
{
	PutData((char*)&value, sizeof(value));

	return *this;
}

CPacket& CPacket::operator>>(unsigned char& value)
{
	GetData((char*)&value, sizeof(unsigned char));

	return *this;
}

CPacket& CPacket::operator>>(char& value)
{
	GetData((char*)&value, sizeof(char));

	return *this;
}

CPacket& CPacket::operator>>(short& value)
{
	GetData((char*)&value, sizeof(short));

	return *this;
}

CPacket& CPacket::operator>>(unsigned short& value)
{
	GetData((char*)&value, sizeof(unsigned short));

	return *this;
}

CPacket& CPacket::operator>>(int& value)
{
	GetData((char*)&value, sizeof(int));

	return *this;
}

CPacket& CPacket::operator>>(unsigned int& value)
{
	GetData((char*)&value, sizeof(unsigned int));

	return *this;
}

CPacket& CPacket::operator>>(long& value)
{
	GetData((char*)&value, sizeof(long));

	return *this;
}

CPacket& CPacket::operator>>(unsigned long& value)
{
	GetData((char*)&value, sizeof(unsigned long));

	return *this;
}

CPacket& CPacket::operator>>(float& value)
{
	GetData((char*)&value, sizeof(float));

	return *this;
}

CPacket& CPacket::operator>>(__int64& value)
{
	GetData((char*)&value, sizeof(__int64));

	return *this;
}

CPacket& CPacket::operator>>(double& value)
{
	GetData((char*)&value, sizeof(double));

	return *this;
}



int CPacket::GetData(char* dest, int size)
{
	if (size > data_size) return 0;

	memcpy(dest, buffer + read_pos, size);
	read_pos += size;
	data_size -= size;

	return size;
}

int CPacket::PutData(char* src, int size)
{
	if (buffer_size <= write_pos + size) return 0;

	memcpy(buffer + write_pos, src, size);
	write_pos += size;
	data_size += size;

	return size;
}

char* CPacket::GetBufferPtrLan(void)
{
	LanPacketHeader* header = (LanPacketHeader*)((char*)buffer - sizeof(LanPacketHeader));
	header->len = data_size;
	return (char*)header;

}

int CPacket::GetDataSizeLan(void)
{
	return data_size + sizeof(LanPacketHeader);
}

char* CPacket::GetBufferPtrNet(void)
{
	NetPacketHeader* header = (NetPacketHeader*)((char*)buffer - sizeof(NetPacketHeader));
	header->len = data_size;

	return (char*)header;
}

int CPacket::GetDataSizeNet(void)
{
	return data_size + sizeof(NetPacketHeader);
}


void CPacket::Encode()
{
	NetPacketHeader* header = (NetPacketHeader*)((char*)buffer - sizeof(NetPacketHeader));
	

	
	unsigned char* buf = (unsigned char*)buffer;

	unsigned char r_key = rand();

	unsigned char p = 0;
	unsigned char e = 0;

	unsigned char sum = 0;

	for (int i = 0; i < data_size; i++)
	{
		sum += buf[i];
	}

	header->check_sum = sum;
	buf = (unsigned char*)&header->check_sum; // 여기부터 해야함
	for (int i = 0; i <= data_size; i++)
	{
		p = buf[i] ^ (p + r_key + i + 1);
		e = p ^ (e + packet_key + i + 1);
		buf[i] = e;
	}

	header->code = packet_code;
	header->len = data_size;
	header->rand_key = r_key;

}


bool CPacket::Decode()
{
	NetPacketHeader* header = (NetPacketHeader*)((char*)buffer - sizeof(NetPacketHeader));

	unsigned char* buf = (unsigned char*)&header->check_sum;

	unsigned char r_key = header->rand_key;
	unsigned char old_p = 0;
	unsigned char p = 0;
	unsigned char d = 0;
	unsigned char e = 0;

	for (int i = 0; i <= data_size; i++)
	{
		p = buf[i] ^ (e + packet_key + i + 1);
		e = buf[i];
		d = p ^ (old_p + r_key + i + 1);
		old_p = p;
		buf[i] = d;
	}

	unsigned char sum = 0;

	buf = (unsigned char*)buffer;
	for (int i = 0; i < data_size; i++)
		sum += buf[i];

	if (sum != header->check_sum)
		return false;

	return true;
}
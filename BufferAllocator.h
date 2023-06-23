#pragma once

#include <map>


/*
 thread unsafe 
 Alloc , Free�� ���� ������, ����ÿ� �� ��..

 
 2048, 4096 byte�� ���۸�
 VirtualAlloc�� �̿��� ��ȯ.
 
 ���۷� ���Ǵ� Page�� ���� ���̱� ����

 BufferAllocator ba;
 ba.Alloc(2000);

 2048byte ����
  
*/
class BufferAllocator
{
public:
	BufferAllocator();
	virtual ~BufferAllocator();
	char* Alloc(const int size);
	void Free(void* const addr);



private:

	char* allocByAddr(char** addr, int size);
	void fixDecommittedAddr(char** addr, char* decommittedAddr);
	void fixReleasedAddr(char** addr, char* releasedAddr);

	char* m_addr2048;
	char* m_addr4096;
	
	std::map<char*, short> m_reserveMap;
	std::map<char*, short> m_commitMap;

};


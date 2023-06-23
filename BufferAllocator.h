#pragma once

#include <map>


/*
 thread unsafe 
 Alloc , Free는 각각 구동시, 종료시에 할 것..

 
 2048, 4096 byte의 버퍼를
 VirtualAlloc을 이용해 반환.
 
 버퍼로 사용되는 Page의 수를 줄이기 위함

 BufferAllocator ba;
 ba.Alloc(2000);

 2048byte 제공
  
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


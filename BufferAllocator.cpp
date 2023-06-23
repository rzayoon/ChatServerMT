#include "BufferAllocator.h"
#include <Windows.h>

constexpr unsigned __int64 PAGE_SIZE = 4096;
constexpr unsigned __int64 GRANULARITY = 65536;
constexpr unsigned __int64 HALF_PAGE = 2048;

using std::map;

BufferAllocator::BufferAllocator()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	if (si.dwPageSize != PAGE_SIZE || si.dwAllocationGranularity != GRANULARITY)
	{
		// 로그
		// 
		int* a = 0;
		*a = 0;
	}

	m_addr2048 = nullptr;
	m_addr4096 = nullptr;

}

BufferAllocator::~BufferAllocator()
{
	if (m_reserveMap.size() || m_commitMap.size())
	{
		// 반환되지 않은 리소스 존재
		int* a = 0;
		*a = 0;
	}

}

char* BufferAllocator::Alloc(const int size)
{
	if (size <= 0 || size > PAGE_SIZE)
	{
		//Crash
		int* a = 0;
		*a = 0;
	}

	char* allocAddr = nullptr;
	if (size <= HALF_PAGE)
	{
		allocAddr = allocByAddr(&m_addr2048, HALF_PAGE);

	}
	else if(size <= PAGE_SIZE)
	{
		allocAddr = allocByAddr(&m_addr4096, PAGE_SIZE);
		
	}


	return allocAddr;
}

void BufferAllocator::Free(void* const addr)
{
	unsigned __int64 addrBit = reinterpret_cast<unsigned __int64>(addr);
	char* reserveAddr = reinterpret_cast<char*>(addrBit & ~(GRANULARITY - 1));
	char* commitAddr = reinterpret_cast<char*>(addrBit & ~(PAGE_SIZE - 1));

	auto iter = m_commitMap.find(commitAddr);
	if (iter == m_commitMap.end())
	{
		// 잘못된 주소
		int* a = 0;
		*a = 0;
		return;
	}


	int remainNumCommit = --m_commitMap[commitAddr];
	if (remainNumCommit == 0)
	{
		VirtualFree(commitAddr, PAGE_SIZE, MEM_DECOMMIT);
		m_commitMap.erase(commitAddr);

		// 다음 주소 할당 지점이 디커밋한 페이지 안인 경우 보정
		fixDecommittedAddr(&m_addr2048, commitAddr);
		fixDecommittedAddr(&m_addr4096, commitAddr);



		int remainNumReserve = --m_reserveMap[reserveAddr];
		if (remainNumReserve == 0)
		{
			VirtualFree(reserveAddr, 0, MEM_RELEASE);
			m_reserveMap.erase(reserveAddr);

			// 할당 지점이 릴리즈 된 영역 내부인 경우 보정
			fixReleasedAddr(&m_addr2048, reserveAddr);
			fixReleasedAddr(&m_addr4096, reserveAddr);

		}
	}


	return;
}

char* BufferAllocator::allocByAddr(char** addr, int size)
{
	char* allocAddr = nullptr;

	unsigned __int64 addrBit = reinterpret_cast<unsigned __int64>(*addr);

	if (*addr == nullptr || (addrBit & (GRANULARITY - 1)) == 0)
	{
		*addr = reinterpret_cast<char*>(VirtualAlloc(nullptr, GRANULARITY, MEM_RESERVE, PAGE_NOACCESS));
		m_reserveMap.insert({ *addr, 0 });

	}


	addrBit = reinterpret_cast<unsigned __int64>(*addr);
	if ((addrBit & (PAGE_SIZE - 1)) == 0)
	{
		allocAddr = reinterpret_cast<char*>(VirtualAlloc(*addr, size, MEM_COMMIT, PAGE_READWRITE));
		char* reserveMapIdx = reinterpret_cast<char*>(addrBit & ~(GRANULARITY - 1));
		m_reserveMap[reserveMapIdx]++;
		m_commitMap.insert({ *addr, 1 });
	}
	else
	{
		allocAddr = *addr;
		char* commitMapIdx = reinterpret_cast<char*>(addrBit & ~(PAGE_SIZE - 1));
		m_commitMap[commitMapIdx]++;
	}
	*addr += size;

	return allocAddr;
}

void BufferAllocator::fixDecommittedAddr(char** addr, char* decommittedAddr)
{
	if ((reinterpret_cast<unsigned __int64>(*addr) - reinterpret_cast<unsigned __int64>(decommittedAddr)) < PAGE_SIZE)
	{
		*addr = decommittedAddr + PAGE_SIZE;
	}

}

void BufferAllocator::fixReleasedAddr(char** addr, char* releasedAddr)
{
	if ((reinterpret_cast<unsigned __int64>(*addr) - reinterpret_cast<unsigned __int64>(releasedAddr)) < GRANULARITY)
	{
		*addr = nullptr;
	}

}

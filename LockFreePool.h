#pragma once
#include <Windows.h>

#include <new>
#include <stdio.h>

#include "CrashDump.h"
//#define LOCKFREE_DEBUG


#define dfPAD -1
#define dfADDRESS_MASK 0x00007fffffffffff
#define dfADDRESS_BIT 47

template <class DATA>
class LockFreePool
{
	template <class DATA> friend class LockFreeQueue;
	template <class DATA> friend class LockFreeStack;


	struct BLOCK_NODE
	{

		BLOCK_NODE* next;
		DATA data; // LockFree 자료구조의 노드 자료구조 밖에서는 이 주소 모름.
		BLOCK_NODE()
		{
			next = nullptr;
		}
	};

private:
	/// <summary>
	/// 생성자 
	/// </summary>
	/// <param name="block_num"> 가용 공간 </param>
	/// <param name="free_list"> true면 capacity 추가 기본값 true </param>
	LockFreePool(int capacity = 0, bool freeList = true);

	virtual ~LockFreePool();

	/// <summary>
	/// 블럭 하나 할당 
	/// 
	/// </summary>
	/// <param name=""></param>
	/// <returns>할당된 DATA 포인터</returns>
	DATA* Alloc(void);

	/// <summary>
	/// 사용중이던 블럭 해제
	/// </summary>
	/// <param name="data"> 반환할 블럭 포인터</param>
	/// <returns> 해당 Pool의 블럭이 아닐 시 false 그 외 true </returns>
	bool Free(DATA* data);

	/// <summary>
	/// 
	/// 
	/// </summary>
	/// <returns>오브젝트 풀에서 생성한 전체 블럭 수</returns>
	int GetCapacity()
	{
		return _capacity;
	}
	/// <summary>
	/// 
	/// </summary>
	/// <returns>오브젝트 풀에서 제공한 블럭 수</returns>
	int GetUseCount()
	{
		return _useCount;
	}

protected:


	BLOCK_NODE* top;

	alignas(64) unsigned int _useCount;
	alignas(64) unsigned int _capacity;
	// 읽기 전용
	alignas(64) bool _freeList;
};

template<class DATA>
LockFreePool<DATA>::LockFreePool(int capacity, bool freeList)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	if ((_int64)si.lpMaximumApplicationAddress != 0x00007ffffffeffff)
	{
		wprintf(L"Maximum Application Address Fault\n");
		CrashDump::Crash();
	}

	_capacity = capacity;
	_freeList = freeList;
	_useCount = 0;


	for (int i = 0; i < capacity; i++)
	{
		BLOCK_NODE* temp = (BLOCK_NODE*)_aligned_malloc(sizeof(BLOCK_NODE), alignof(BLOCK_NODE));



		temp->next = top;
		top = temp;
	}

}

template<class DATA>
LockFreePool<DATA>::~LockFreePool()
{
	// todo : 블럭 생성시 따로 리스트에 보관해서 삭제하기
	BLOCK_NODE* top_addr = (BLOCK_NODE*)((unsigned long long)top & dfADDRESS_MASK);
	BLOCK_NODE* temp;

	while (top_addr)
	{
		temp = top_addr->next;
		_aligned_free(top_addr);
		top_addr = temp;
	}

}

template<class DATA>
DATA* LockFreePool<DATA>::Alloc()
{
	unsigned long long old_top;  // 비교용
	BLOCK_NODE* old_top_addr; // 실제 주소
	BLOCK_NODE* next; // 다음 top
	BLOCK_NODE* new_top;

	

	while (1)
	{
		old_top = (unsigned long long)top;
		old_top_addr = (BLOCK_NODE*)(old_top & dfADDRESS_MASK);

		if (old_top_addr == nullptr)
		{
			if (_freeList)
			{
				InterlockedIncrement(&_capacity);

				old_top_addr = (BLOCK_NODE*)_aligned_malloc(sizeof(BLOCK_NODE), alignof(BLOCK_NODE));
	
				old_top_addr->next = nullptr;

				break;
			}
			else
			{
				return nullptr;
			}
		}


		unsigned long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;
		next = old_top_addr->next;

		new_top = (BLOCK_NODE*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));


		if (old_top == (unsigned long long)InterlockedCompareExchangePointer((PVOID*)&top, (PVOID)new_top, (PVOID)old_top))
		{
			
			break;
		}
	}

	InterlockedIncrement((LONG*)&_useCount);

	return &old_top_addr->data;
	
}

template<class DATA>
bool LockFreePool<DATA>::Free(DATA* data)
{
	unsigned long long old_top;


	BLOCK_NODE* node = (BLOCK_NODE*)((unsigned long long)data - alignof(DATA));
	BLOCK_NODE* old_top_addr;
	PVOID new_top;


	while (1)
	{
		old_top = (unsigned long long)top;
		old_top_addr = (BLOCK_NODE*)(old_top & dfADDRESS_MASK);

		unsigned long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;

		node->next = old_top_addr;

		new_top = (BLOCK_NODE*)((unsigned long long)node | (next_cnt << dfADDRESS_BIT));

		if (old_top == (unsigned long long)InterlockedCompareExchangePointer((PVOID*)&top, (PVOID)new_top, (PVOID)old_top))
		{
			InterlockedDecrement((LONG*)&_useCount);
			break;
		}
	}
	
	return true;
}
#pragma once
#include <Windows.h>

#include <new>
#include <stdio.h>

//#define LOCKFREE_DEBUG


#define dfPAD -1
#define dfADDRESS_MASK 0x00007fffffffffff
#define dfADDRESS_BIT 47

template <class DATA>
class LockFreePool
{
	template <class DATA> friend class LockFreeQueue;
	template <class DATA> friend class LockFreeStack;

#ifdef LOCKFREE_DEBUG
	enum
	{
		PAD = 0xABCDABCDABCDABCD
	};
#endif

	struct BLOCK_NODE
	{
#ifdef LOCKFREE_DEBUG
		unsigned long long front_pad;
#endif
		DATA data;
#ifdef LOCKFREE_DEBUG
		unsigned long long back_pad;
#endif
		BLOCK_NODE* next;
	

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

	void Crash()
	{
		int* a = (int*)0;
		*a = 0;

	}

	BLOCK_NODE* top;
#ifdef LOCKFREE_DEBUG
	int padding_size;
#endif
	alignas(64) unsigned int _useCount;
	alignas(64) unsigned int _capacity;
	// 읽기 전용
	alignas(64) bool _freeList;
	alignas(64) char b; // 클래스 뒤에 뭐가 올지 모름 (자주 변경되는 값? - > freelist 읽을 때 캐시무효화)
};

template<class DATA>
LockFreePool<DATA>::LockFreePool(int capacity, bool freeList)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	if ((_int64)si.lpMaximumApplicationAddress != 0x00007ffffffeffff)
	{
		wprintf(L"Maximum Application Address Fault\n");
		Crash();
	}

	_capacity = capacity;
	_freeList = freeList;
	_useCount = 0;

#ifdef LOCKFREE_DEBUG
	padding_size = max(sizeof(BLOCK_NODE::front_pad), alignof(DATA));
#endif

	for (int i = 0; i < capacity; i++)
	{
		BLOCK_NODE* temp = (BLOCK_NODE*)_aligned_malloc(sizeof(BLOCK_NODE), alignof(BLOCK_NODE));

#ifdef LOCKFREE_DEBUG
		temp->front_pad = PAD;
		temp->back_pad = PAD;
#endif

		temp->next = top;
		top = temp;
	}

}

template<class DATA>
LockFreePool<DATA>::~LockFreePool()
{
	// 반환 안된 노드는 포기.. 어차피 전역에 놓고 쓸거라 
	// 소멸 시점은 프로그램 종료 때
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
	
#ifdef LOCKFREE_DEBUG
				old_top_addr->front_pad = PAD;
				old_top_addr->back_pad = PAD;
#endif LOCKFREE_DEBUG
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

#ifdef LOCKFREE_DEBUG
	// data가 8바이트 초과하면 문제될 수 있음 구조체 패딩 관련
	BLOCK_NODE* node = (BLOCK_NODE*)((char*)data - padding_size); 
	if (node->front_pad != PAD || node->back_pad != PAD) // 사용 중에 버퍼 오버런 있었는지 체크용
	{
		Crash();
	}
#else
	BLOCK_NODE* node = (BLOCK_NODE*)data;
#endif
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
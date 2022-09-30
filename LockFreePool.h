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

public:
	/// <summary>
	/// 생성자 
	/// </summary>
	/// <param name="block_num"> 초기 블럭 수 </param>
	/// <param name="free_list"> capacity 추가 여부 </param>
	LockFreePool(int _block_num, bool _free_list = true);

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
		return capacity;
	}
	/// <summary>
	/// 
	/// </summary>
	/// <returns>오브젝트 풀에서 제공한 블럭 수</returns>
	int GetUseCount()
	{
		return use_count;
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
	alignas(64) unsigned int use_count;
	alignas(64) unsigned int capacity;
	alignas(64) bool free_list;

};

template<class DATA>
LockFreePool<DATA>::LockFreePool(int _capacity, bool _free_list)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	if ((_int64)si.lpMaximumApplicationAddress != 0x00007ffffffeffff)
	{
		wprintf(L"Maximum Application Address Fault\n");
		Crash();
	}

	capacity = _capacity;
	free_list = _free_list;
	use_count = 0;

#ifdef LOCKFREE_DEBUG
	padding_size = max(sizeof(BLOCK_NODE::front_pad), alignof(DATA));
#endif

	for (unsigned int i = 0; i < capacity; i++)
	{
		BLOCK_NODE* temp = (BLOCK_NODE*)_aligned_malloc(sizeof(BLOCK_NODE), alignof(BLOCK_NODE));
		ZeroMemory(&temp->data, sizeof(DATA));
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
			if (free_list)
			{
				InterlockedIncrement(&capacity);

				old_top_addr = (BLOCK_NODE*)_aligned_malloc(sizeof(BLOCK_NODE), alignof(BLOCK_NODE));
				ZeroMemory(&old_top_addr->data, sizeof(DATA));
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

	InterlockedIncrement((LONG*)&use_count);

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
			InterlockedDecrement((LONG*)&use_count);
			break;
		}
	}
	
	return true;
}
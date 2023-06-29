#pragma once

#/*include <stdlib.h>
#include <stdio.h>

#include "LockFreeQueue.h"*/
#include "LockFreeStack.h"

#pragma warning(push)
#pragma warning(disable: 6011)


template <class DATA>
class MemoryPoolTls
{
	enum
	{
		DEFAULT_SIZE = 200
	};

	struct BLOCK_NODE
	{
		BLOCK_NODE* next = nullptr;
		DATA data;
	};


	class POOL
	{
	public:

		POOL(int _init_size, int _default_size, bool _placement_new)
		{
			default_size = _default_size;
			placement_new = _placement_new;

			Generate(_init_size);
		}

		~POOL()
		{
			BLOCK_NODE* node;
			if (placement_new)
			{
				while (top)
				{
					node = top;
					top = top->next;
					_aligned_free(node);
				}
			}
			else
			{
				while (top)
				{
					node = top;
					top = top->next;
					delete node;
				}

			}
		}
		DATA* Alloc()
		{
	
			BLOCK_NODE* node = top;
			top = top->next;
			size--;

			return &node->data;
		}

		
		void Free(DATA* data)
		{
			size++;
			BLOCK_NODE* node = (BLOCK_NODE*)((unsigned long long)data - alignof(DATA));
			node->next = top;
			top = node;

			return;
		}

		void Generate(int _size)
		{
			size = _size;
			if (placement_new)
			{
				for (int i = 0; i < size; i++)
				{
					BLOCK_NODE* temp = (BLOCK_NODE*)_aligned_malloc(sizeof(BLOCK_NODE), alignof(BLOCK_NODE));

					temp->next = top;
					top = temp;

				}
			}
			else
			{
				for (int i = 0; i < size; i++)
				{
					BLOCK_NODE* temp = new BLOCK_NODE;

					temp->next = top;
					top = temp;
				}
			}

		}

		int GetSize()
		{
			return size;
		}

		void Clear()
		{
			top = nullptr;
			size = 0;
		}

		BLOCK_NODE* top;

		int size;
		alignas(64) int default_size;
		bool placement_new;
	};


	struct THREAD_DATA
	{
		POOL* pool;
		POOL* chunk; // 초과분 모으는 용도
	};

public:


	MemoryPoolTls(int _default_size = DEFAULT_SIZE, bool _placement_new = false)
	{
		tls_index = TlsAlloc();
		if (tls_index == TLS_OUT_OF_INDEXES)
			wprintf(L"%d tls error\n", GetLastError());
		placement_new = _placement_new;

		alloc_size = 0;
		use_size = 0;
		default_size = _default_size;
	}

	virtual ~MemoryPoolTls()
	{
		BLOCK_NODE* chunk;
		while (chunk_pool.Pop(&chunk))
		{
			BLOCK_NODE* temp;
			if (placement_new)
			{
				while (chunk)
				{
					temp = chunk;
					chunk = temp->next;
					_aligned_free(temp);
				}
			}
			else
			{
				while (chunk)
				{
					temp = chunk;
					chunk = temp->next;
					delete temp;
				}

			}
		}
	}



	DATA* Alloc()
	{
		// TLS로 변경 가능한 부분
		THREAD_DATA* td = (THREAD_DATA*)TlsGetValue(tls_index);
		if (td == nullptr)
		{
			td = new THREAD_DATA;
			td->pool = GeneratePool(default_size);
			td->chunk = GeneratePool(0);
			TlsSetValue(tls_index, (LPVOID)td);
		}


		BLOCK_NODE* chunk_top;
		// 할당
		POOL* td_pool = td->pool;
		DATA* ret;
		if (td_pool->size == 0) // 풀 다 쓴 경우
		{
			POOL* td_chunk = td->chunk;
			if (td_chunk->size != 0) // 스레드에서 모은거 사용
			{
				td_pool->top = td_chunk->top;
				td_pool->size = td_chunk->size;
				td_chunk->top = nullptr;
				td_chunk->size = 0;
			}
			else if (chunk_pool.Pop(&chunk_top))
			{ // 가용 청크 가져옴
				
				td_pool->top = chunk_top;
				td_pool->size = default_size;

			}
			else // 모아둔 청크도 없음.
			{
				GeneratePool(td_pool); // 생성
			}
		}
		ret = td_pool->Alloc();
		
		if (placement_new)
			new(ret) DATA;

		InterlockedIncrement((LONG*)&use_size);
		

		return ret;
	}

	bool Free(DATA* data)
	{
		if (data == nullptr) return false;

		THREAD_DATA* td = (THREAD_DATA*)TlsGetValue(tls_index);
		if (td == nullptr)
		{
			td = new THREAD_DATA;
			td->pool = GeneratePool(default_size);
			td->chunk = GeneratePool(0);
			TlsSetValue(tls_index, (LPVOID)td);
		}
		int size = default_size;
		POOL* td_pool = td->pool;
		POOL* td_chunk = td->chunk;

		if (placement_new)
			data->~DATA();

		if (td_pool->size == size) //풀 초과분
		{

			if (td_chunk->size == size) //청크도 꽉참
			{
				chunk_pool.Push(td_chunk->top);
				td_chunk->Clear();	
			}

			td_chunk->Free(data);
		}
		else
		{
			td_pool->Free(data);
		}

		InterlockedDecrement((LONG*)&use_size);

		return true;
	}

	int GetUseSize()
	{
		return use_size;
	}

	int GetAllocSize()
	{
		return alloc_size;
	}

private:

	POOL* GeneratePool(int _init_size)
	{
		InterlockedAdd((LONG*)&alloc_size, _init_size);
		return new POOL(_init_size, default_size, placement_new);
	}

	void GeneratePool(POOL* _pool)
	{
		InterlockedAdd((LONG*)&alloc_size, default_size);
		_pool->Generate(default_size);
	}


	LockFreeStack<BLOCK_NODE*> chunk_pool = LockFreeStack<BLOCK_NODE*>(0);

	alignas(64) int use_size;
	alignas(64) int alloc_size;
	alignas(64) int tls_index;
	bool placement_new;
	int default_size;
};

#pragma warning(pop)
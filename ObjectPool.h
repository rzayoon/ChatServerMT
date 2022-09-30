#pragma once
#include <Windows.h>
/*---------------------------------------------------------------

	procademy MemoryPool.

	�޸� Ǯ Ŭ���� (������Ʈ Ǯ / ��������Ʈ)
	Ư�� ����Ÿ(����ü,Ŭ����,����)�� ������ �Ҵ� �� ��������.

	- ����.

	ObjectPool<DATA> Pool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData ���

	Pool.Free(pData);


----------------------------------------------------------------*/




template <class DATA>
class ObjectPool
{
	enum {
		PAD = 0xABCDABCD
	};

	struct BLOCK_NODE
	{
		unsigned int front_pad;
		DATA data;
		unsigned int back_pad;
		BLOCK_NODE* next;
	};


public:
	/// <summary>
	/// ������ 
	/// </summary>
	/// <param name="block_num"> �ʱ� �� �� </param>
	/// <param name="placement_new"> Alloc �Ǵ� Free �� ������ ȣ�� ���� </param>
	ObjectPool(int block_num, bool placement_new = false);

	virtual ~ObjectPool();

	/// <summary>
	/// �� �ϳ� �Ҵ� 
	/// 
	/// </summary>
	/// <param name=""></param>
	/// <returns>�Ҵ�� DATA ������</returns>
	DATA* Alloc(void);

	/// <summary>
	/// ������̴� �� ����
	/// </summary>
	/// <param name="data"> ��ȯ�� �� ������</param>
	/// <returns> �ش� Pool�� ���� �ƴ� �� false �� �� true </returns>
	bool Free(DATA* data);

	/// <summary>
	/// 
	/// 
	/// </summary>
	/// <returns>������Ʈ Ǯ���� ������ ��ü �� ��</returns>
	int GetCapacity()
	{
		return capacity;
	}
	/// <summary>
	/// 
	/// </summary>
	/// <returns>������Ʈ Ǯ���� ������ �� ��</returns>
	int GetUseCount()
	{
		return use_count;
	}

	void Lock()
	{
		AcquireSRWLockExclusive(&pool_srw);
	}

	void Unlock()
	{
		ReleaseSRWLockExclusive(&pool_srw);
	}

protected:
	BLOCK_NODE* top;
	
	int capacity;
	int use_count;
	SRWLOCK pool_srw;
	
	alignas(64) BLOCK_NODE** pool;
	bool placement_new;
	int padding_size;

};

template<class DATA>
ObjectPool<DATA>::ObjectPool(int _capacity, bool _placement_new)
{
	capacity = _capacity;
	use_count = 0;
	placement_new = _placement_new;
	InitializeSRWLock(&pool_srw);

	padding_size = max(sizeof(BLOCK_NODE::front_pad), alignof(DATA));

	pool = new BLOCK_NODE*[capacity];

	if (placement_new)
	{
		for (int i = 0; i < capacity; i++)
		{
			BLOCK_NODE* temp = (BLOCK_NODE*)malloc(sizeof(BLOCK_NODE));
			temp->front_pad = PAD;
			temp->back_pad = PAD;

			temp->next = top;
			top = temp;

			pool[i] = temp;
		}
	}
	else
	{
		for (int i = 0; i < capacity; i++)
		{
			BLOCK_NODE* temp = new BLOCK_NODE;
			temp->front_pad = PAD;
			temp->back_pad = PAD;

			temp->next = top;
			top = temp;

			pool[i] = temp;

		}
	}
}

template<class DATA>
ObjectPool<DATA>::~ObjectPool()
{
	if (placement_new)
	{
		for (int i = 0; i < capacity; i++)
		{
			free(pool[i]);
		}
	}
	else
	{
		for (int i = 0; i < capacity; i++)
		{
			delete pool[i];
		}
	}
	delete[] pool;

}

template<class DATA>
DATA* ObjectPool<DATA>::Alloc()
{
	if (use_count == capacity)
	{
		BLOCK_NODE** new_pool = new BLOCK_NODE * [capacity * 2];
		memcpy_s(new_pool, sizeof(BLOCK_NODE*) * capacity, pool, sizeof(BLOCK_NODE*) * capacity);
		delete[] pool;
		pool = new_pool;
		int new_capa = capacity * 2;
		
		BLOCK_NODE* next = nullptr;
		if (placement_new)
		{
			for (int i = capacity; i < new_capa; i++)
			{
				BLOCK_NODE* temp = (BLOCK_NODE*)malloc(sizeof(BLOCK_NODE));
				temp->front_pad = PAD;
				temp->back_pad = PAD;

				temp->next = top;
				top = temp;

				pool[i] = temp;
			}
		}
		else
		{
			for (int i = capacity; i < new_capa; i++)
			{
				BLOCK_NODE* temp = new BLOCK_NODE;
				temp->front_pad = PAD;
				temp->back_pad = PAD;

				temp->next = top;
				top = temp;

				pool[i] = temp;
			}
		}
		capacity = new_capa;
	}

	DATA* ret = &top->data;
	top = top->next;
	use_count++;
	if (placement_new)
		new(ret) DATA;

	return ret;
}

template<class DATA>
bool ObjectPool<DATA>::Free(DATA* data)
{
	if (placement_new)
		data->~DATA();

	BLOCK_NODE* node = (BLOCK_NODE*)((char*)data - padding_size);
	if (node->front_pad != PAD || node->back_pad != PAD)
	{
		int* a = 0;
		*a = 0;
	}


	node->next = top;
	top = node;
	use_count--;



	return true;
}
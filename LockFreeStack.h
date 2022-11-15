#pragma once
#include "LockFreePool.h"



template<class T>
class LockFreeStack
{
	struct Node
	{
		T data;
		Node* next;

		Node()
		{
			next = nullptr;
		}

		~Node()
		{

		}
	};


public:

	LockFreeStack(unsigned int size = 10000, bool freeList = true);
	~LockFreeStack();

	bool Push(T data);
	bool Pop(T* data);
	long long GetSize();


private:

	alignas(64) Node* _top;
	alignas(64) LONG64 _size;
	alignas(64) bool _freeList;
	LockFreePool<Node> *_pool;
	alignas(64) char b;

};

template<class T>
inline LockFreeStack<T>::LockFreeStack(unsigned int size, bool freeList)
{
	_size = 0;
	_top = nullptr;
	_freeList = freeList;
	_pool = new LockFreePool<Node>(size, freeList);
}

template<class T>
inline LockFreeStack<T>::~LockFreeStack()
{
	delete _pool;
}

template<class T>
inline bool LockFreeStack<T>::Push(T data)
{
	Node* node = _pool->Alloc();
	if (node == nullptr)
		return false;


	node->data = data;

	
	unsigned long long old_top;
	Node* old_top_addr;
	Node* new_top;

	while (1)
	{
		old_top = (unsigned long long)_top;
		old_top_addr = (Node*)(old_top & dfADDRESS_MASK);
		unsigned long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;
		node->next = old_top_addr;

		new_top = (Node*)((unsigned long long)node | (next_cnt << dfADDRESS_BIT));

		if (old_top == (unsigned long long)InterlockedCompareExchangePointer((PVOID*)&_top, new_top, (PVOID)old_top))
		{
			InterlockedIncrement64(&_size);
			break;
		}

	}

	return true;

} 

template<class T>
inline bool LockFreeStack<T>::Pop(T* data)
{
	unsigned long long old_top;
	Node* old_top_addr;
	Node* next;
	Node* new_top;

	while (1)
	{
		old_top = (unsigned long long)_top;
		old_top_addr = (Node*)(old_top & dfADDRESS_MASK);
		if (old_top_addr == nullptr)
		{
			return false;
		};

		unsigned long long next_cnt = (old_top >> dfADDRESS_BIT) + 1;
		next = old_top_addr->next;

		new_top = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));

		if (old_top == (unsigned long long)InterlockedCompareExchangePointer((PVOID*)&_top, (PVOID)new_top, (PVOID)old_top))
		{
			InterlockedDecrement64(&_size);
			*data = old_top_addr->data;
			
			old_top_addr->data.~T();
			_pool->Free(old_top_addr);

			break;
		}
	}

	return true;
}

template<class T>
inline long long LockFreeStack<T>::GetSize()
{
	return _size;
}

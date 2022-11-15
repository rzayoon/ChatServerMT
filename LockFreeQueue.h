#pragma once
#include <Windows.h>
#include "LockFreePool.h"



template<class T>
class LockFreeQueue
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

	LockFreeQueue(unsigned int capacity = 10000, bool freeList = true);
	~LockFreeQueue();

	bool Enqueue(T data);
	bool Dequeue(T* data);
	long long GetSize();


private:

	alignas(64) Node* _tail;
	alignas(64) Node* _head;
	alignas(64) LONG64 _size;
	LockFreePool<Node>* _pool;
	bool _freeList;
};

template<class T>
inline LockFreeQueue<T>::LockFreeQueue(unsigned int capacity, bool free_list)
{
	_size = 0;
	_freeList = free_list;
	_pool = new LockFreePool<Node>(capacity + 1, _freeList);
	Node* dummy = _pool->Alloc();
	dummy->next = nullptr;
	_head = dummy;
	
	_tail = _head;
}

template<class T>
inline LockFreeQueue<T>::~LockFreeQueue()
{
	delete _pool;
}

template<class T>
inline bool LockFreeQueue<T>::Enqueue(T data)
{
	Node* node = _pool->Alloc();
	if (node == nullptr)
		return false;

	unsigned long long old_tail;
	Node* tail;
	Node* new_tail;
	Node* next = nullptr;
	unsigned long long next_cnt;

	node->data = data;
	node->next = nullptr;

	while (true)
	{
		old_tail = (unsigned long long)_tail;
		tail = (Node*)(old_tail & dfADDRESS_MASK);
		next_cnt = (old_tail >> dfADDRESS_BIT) + 1;


		next = tail->next;



		if (next == nullptr)
		{
			if (InterlockedCompareExchangePointer((PVOID*)&tail->next, node, next) == next)
			{
				if (_tail == (PVOID)old_tail)
				{
					new_tail = (Node*)((unsigned long long)node | (next_cnt << dfADDRESS_BIT));
					InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);
				}
				break;
			}
		}
		else // 아직 tail을 밀어줘야 할 스레드가 안했으면 지금 한다.
		{
			new_tail = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));
			InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);

		}
		
	}
	InterlockedIncrement64(&_size);

	return true;
}

template<class T>
inline bool LockFreeQueue<T>::Dequeue(T* data)
{
	InterlockedDecrement64(&_size);
	unsigned long long old_head;
	Node* head;
	Node* next;
	unsigned long long next_cnt;

	while (true)
	{
		old_head = (unsigned long long)_head;
		head = (Node*)(old_head & dfADDRESS_MASK);
		next_cnt = (old_head >> dfADDRESS_BIT) + 1;



		next = head->next;




		if (next == nullptr)
		{
			// 정말 없었으면 안들어왔어야 함.
			// 그럼에도 원인이 될 수 있는 상황
			// 1. 지금 보고 있는 head가 다른 스레드에서 dequeue되었고 다시 enque과정에서 next가 null로 쓰여졌다.
			// 2. 한 스레드가 enque에서 노드 하나를 tail로 저장하고 잠시 안돌았음
			//    -> 해당 노드가 queue에서 빠져나감 -> 다시 enqueue에서 next = null 됨
			//    -> 멈춰있던 스레드가 동작하면서 next에 이어버림 -> 사이즈 증가
			//    -> 해당 노드를 연결해야하는 스레드가 정지중.
			//    -> 이 때 디큐 시도하는 스레드는 기다리느라 못하는 상태가 됨.
			//    이 문제는 enqueue 중인 스레드가 일을 마치면 해결이 된다.
			//    하지만 다른 스레드 때문에 해야할 일을 못하는 것은 락프리의 목적에 위배된다.
			//    next가 null로 읽히면 1, 2의 상황 구분하지 않고 그냥 없는 것으로 본다..
			InterlockedIncrement64(&_size);
			return false;
		}
		else
		{
			unsigned long long old_tail = (unsigned long long)_tail;
			if (old_head == old_tail) // cnt까지 일치하면 같은 것
			{
				Node* tail = (Node*)(old_tail & dfADDRESS_MASK);
				unsigned long long tail_cnt = (old_tail >> dfADDRESS_BIT) + 1;
				Node* new_tail = (Node*)((unsigned long long)next | (tail_cnt << dfADDRESS_BIT));
				InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);
			}

			Node* new_head = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));

			*data = next->data;
			// data가 객체인 경우.. 느려질 것 사용자의 문제. template type이 복사 비용이 적은 포인터나 일반 타입이었어야 한다.
			if (InterlockedCompareExchangePointer((PVOID*)&_head, new_head, (PVOID)old_head) == (PVOID)old_head)
			{
				head->data.~T(); // 문제!! 더미 노드 하나가 삭제되지 않고 물려버림
				_pool->Free(head);
				break;
			}
		}
	}

	return true;
}

template<class T>
inline long long LockFreeQueue<T>::GetSize()
{
	return _size;
}

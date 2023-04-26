#pragma once
#include <Windows.h>
#include "LockFreePool.h"



template<class T>
class LockFreeQueue
{
	struct Node
	{
		Node* next;
		T data;
		
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
LockFreeQueue<T>::LockFreeQueue(unsigned int capacity, bool free_list)
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
LockFreeQueue<T>::~LockFreeQueue()
{
	delete _pool;
}

template<class T>
bool LockFreeQueue<T>::Enqueue(T data)
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
		else // ���� tail�� �о���� �� �����尡 �������� ���� �Ѵ�.
		{
			new_tail = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));
			InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);

		}
		
	}
	InterlockedIncrement64(&_size);

	return true;
}

template<class T>
bool LockFreeQueue<T>::Dequeue(T* data)
{
	if (_size <= 0)
		return false;

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
			// ���� �������� �ȵ��Ծ�� ��.
			// �׷����� ������ �� �� �ִ� ��Ȳ
			// 1. ���� ���� �ִ� head�� �ٸ� �����忡�� dequeue�Ǿ��� �ٽ� enque�������� next�� null�� ��������.
			// 2. A �����尡 enque���� a ��带 ���� tail�� �����ϰ� ��� �ȵ�����
			//    -> a ��尡 queue���� ��������(pool ��ȯ) -> �ٽ� enqueue(alloc)���� next = null �� ť�� ������ x
			//    -> �����ִ� A �����尡 �����ϸ鼭 �� ��带 a ����� next�� �̾���� -> ������ ����
			//    -> a ��带 ť�� �����ؾ��ϴ� �����尡 ������.
			//    -> �� �� ��ť �õ��ϴ� ������� ��ٸ����� ���ϴ� ���°� �� (������ �����δ� ť�� ���Ұ� ������)
			//    �� ������ enqueue ���� �����尡 ���� ��ġ�� �ذ��� �ȴ�.
			//    ������ �ٸ� ������ ������ �ؾ��� ���� ���ϴ� ���� �������� ������ ����ȴ�.
			//    next�� null�� ������ 1, 2�� ��Ȳ �������� �ʰ� ����� ��� ���� �׳� ���� ������ ����..
			InterlockedIncrement64(&_size);
			return false;
		}
		else
		{
			unsigned long long old_tail = (unsigned long long)_tail;
			if (old_head == old_tail) // cnt���� ��ġ�ϸ� ���� ��
			{
				Node* tail = (Node*)(old_tail & dfADDRESS_MASK);
				unsigned long long tail_cnt = (old_tail >> dfADDRESS_BIT) + 1;
				Node* new_tail = (Node*)((unsigned long long)next | (tail_cnt << dfADDRESS_BIT));
				InterlockedCompareExchangePointer((PVOID*)&_tail, new_tail, (PVOID)old_tail);
			}

			Node* new_head = (Node*)((unsigned long long)next | (next_cnt << dfADDRESS_BIT));

			*data = next->data;
			// data�� ��ü�� ���.. ������ �� ������� ����. template type�� ���� ����� ���� �����ͳ� �Ϲ� Ÿ���̾���� �Ѵ�.
			if (InterlockedCompareExchangePointer((PVOID*)&_head, new_head, (PVOID)old_head) == (PVOID)old_head)
			{
				head->data.~T(); // ����!! ���� ��� �ϳ��� �������� �ʰ� ��������
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

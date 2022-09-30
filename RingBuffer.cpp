#include "RingBuffer.h"
#include <string.h>

RingBuffer::RingBuffer(int _bufSize)
{

	bufSize = _bufSize + 1;
	buf = new char[bufSize];
	fillSize = 0;
	front = 0;
	rear = 0;
	InitializeSRWLock(&srw);
}

RingBuffer::~RingBuffer()
{
	delete[] buf;
}

void RingBuffer::Lock()
{
	AcquireSRWLockExclusive(&srw);
}

void RingBuffer::UnLock()
{
	ReleaseSRWLockExclusive(&srw);
}

int RingBuffer::DirectEnqueSize(void)
{
	unsigned int f_now = front;
	if (fillSize == bufSize - 1) return 0;

	if (rear < f_now)
		return f_now - rear - 1;
	if (front == 0)
		return bufSize - rear - 1;
	return bufSize - rear;
}

int RingBuffer::DirectDequeSize(void)
{
	unsigned int r_now = rear;
	if (fillSize == 0) return 0;


	if (r_now < front)
		return bufSize - front;
	return r_now - front;
}

int RingBuffer::Enqueue(const char* _buf, int _size)
{
	if (_size <= 0) return 0;
	if (_size > bufSize - fillSize - 1)
	{
		return 0;
	}


	if (_size > bufSize - rear)
	{
		unsigned size1 = bufSize - rear;
		unsigned size2 = _size - size1;
		memcpy(buf + rear, _buf, size1);
		memcpy(buf, _buf + size1, size2);
		rear = size2;

		InterlockedAdd((LONG*)&fillSize, _size);

		return _size;
	}
	else
	{
		memcpy(buf + rear, _buf, _size);
		rear += _size;
		if (rear == bufSize) rear = 0;
		InterlockedAdd((LONG*)&fillSize, _size);

		return _size;

	}
}

int RingBuffer::Dequeue(char* _buf, int _size)
{
	if (_size <= 0) return 0;
	if (_size > fillSize)
	{
		return 0;
	}

	if (_size > bufSize - front)
	{
		unsigned size1 = bufSize - front;
		unsigned size2 = _size - size1;
		memcpy(_buf, buf + front, size1);
		memcpy(_buf + size1, buf, size2);
		front = size2;

		InterlockedAdd((LONG*)&fillSize, -1 * _size);
		return _size;
	}
	else
	{
		memcpy(_buf, buf + front, _size);
		front += _size;
		if (front == bufSize) front = 0;

		InterlockedAdd((LONG*)&fillSize, -1 * _size);

		return _size;
	}
}

int RingBuffer::Peek(char* _buf, int _size)
{
	if (_size <= 0) return 0;
	if (_size > fillSize) return 0;


	if (_size > bufSize - front)
	{
		unsigned size1 = bufSize - front;
		unsigned size2 = _size - size1;
		memcpy(_buf, buf + front, size1);
		memcpy(_buf + size1, buf, size2);

		return _size;
	}
	else
	{
		memcpy(_buf, buf + front, _size);
		return _size;
	}
}

void RingBuffer::MoveRear(int _size)
{
	int nowRear = rear;
	nowRear = (nowRear + _size) % bufSize;

	//InterlockedExchange((LONG*)&rear, nowRear);
	rear = nowRear;
	InterlockedAdd((LONG*)&fillSize, _size);
}

void RingBuffer::MoveFront(int _size)
{
	int nowFront = front;
	nowFront = (nowFront + _size) % bufSize;

	//InterlockedExchange((LONG*)&front, nowFront);
	front = nowFront;
	InterlockedAdd((LONG*)&fillSize, -1 * _size);
}


char* RingBuffer::GetFrontPtr(void)
{
	return buf + front;
}

char* RingBuffer::GetRearPtr(void)
{
	return buf + rear;
}


bool RingBuffer::IsFull()
{
	if (fillSize + 1 >= bufSize)
		return true;
	return false;

}

bool RingBuffer::IsEmpty()
{
	if (fillSize == 0) return true;
	return false;
}
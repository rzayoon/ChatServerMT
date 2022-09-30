#pragma once
#include <Windows.h>

class RingBuffer
{
public:
	/// <summary>
	/// ������
	/// </summary>
	/// <param name="_bufSize"> ring buffer byte ���� ũ��, �⺻�� 10000</param>
	RingBuffer(int _bufSize = 10000);

	~RingBuffer();

	void Lock();
	void UnLock();

	/// <summary>
	/// ������� ���� ũ�� ��ȯ
	/// </summary>
	/// <returns></returns>
	int GetFillSize()
	{
		return fillSize;
	}
	/// <summary>
	/// ����ִ� ���� ũ�� ��ȯ
	/// </summary>
	/// <returns></returns>
	int GetEmptySize()
	{
		return bufSize - fillSize - 1;
	}

	/// <summary>
	/// ring buffer array�� �� ���� ���� �� �ִ� ������
	/// </summary>
	/// <returns></returns>
	int DirectEnqueSize();

	/// <summary>
	/// ring buffer array���� �� ���� �� �� �ִ� ������
	/// </summary>
	/// <returns></returns>
	int DirectDequeSize();

	int Enqueue(const char* _buf, int _size);
	int Dequeue(char* _buf, int _size);
	int Peek(char* _buf, int _size);

	/// <summary>
	/// Enqueue�� ���� ������ rear ��ġ�� �ű��.
	/// ����� ������ �������� üũ���� �����Ƿ� ���� ����� ũ�⸦ ���ڷ� �־�� ��.
	/// </summary>
	/// <param name="_size"></param>
	void MoveRear(int _size);

	/// <summary>
	/// Dequeue�� ���� ������ front ��ġ�� �ű��.
	/// ����� ������ �������� üũ���� �����Ƿ� ���� ����� ũ�⸦ ���ڷ� �־�� ��.
	/// </summary>
	/// <param name="_size"></param>
	void MoveFront(int _size);

	/// <summary>
	/// ������ �ʱ�ȭ
	/// </summary>
	void ClearBuffer()
	{
		rear = 0;
		front = 0;
		fillSize = 0;
	}

	/// <summary>
	/// front�� ������ ��ȯ
	/// </summary>
	/// <returns></returns>
	char* GetFrontPtr();
	/// <summary>
	/// rear�� ������ ��ȯ
	/// </summary>
	/// <returns></returns>
	char* GetRearPtr();

	char* GetBufPtr()
	{
		return buf;
	}

	bool IsFull();
	bool IsEmpty();

private:

	// �б� ����
	char* buf;
	unsigned int bufSize;
	
	// ���� �ٸ� �����忡�� ���� �� ����. ��ȿȭ ����
	alignas(64) unsigned int front;
	alignas(64) unsigned int rear;

	alignas(64) int fillSize;
	alignas(64)	SRWLOCK srw;
};


#pragma once
#include <Windows.h>

class RingBuffer
{
public:
	/// <summary>
	/// 생성자
	/// </summary>
	/// <param name="_bufSize"> ring buffer byte 단위 크기, 기본값 10000</param>
	RingBuffer(int _bufSize = 10000);

	~RingBuffer();

	void Lock();
	void UnLock();

	/// <summary>
	/// 사용중인 버퍼 크기 반환
	/// </summary>
	/// <returns></returns>
	int GetFillSize()
	{
		return fillSize;
	}
	/// <summary>
	/// 비어있는 버퍼 크기 반환
	/// </summary>
	/// <returns></returns>
	int GetEmptySize()
	{
		return bufSize - fillSize - 1;
	}

	/// <summary>
	/// ring buffer array에 한 번에 넣을 수 있는 사이즈
	/// </summary>
	/// <returns></returns>
	int DirectEnqueSize();

	/// <summary>
	/// ring buffer array에서 한 번에 뺄 수 있는 사이즈
	/// </summary>
	/// <returns></returns>
	int DirectDequeSize();

	int Enqueue(const char* _buf, int _size);
	int Dequeue(char* _buf, int _size);
	int Peek(char* _buf, int _size);

	/// <summary>
	/// Enqueue된 내부 버퍼의 rear 위치를 옮긴다.
	/// 사이즈가 가능한 범위인지 체크하지 않으므로 실제 복사된 크기를 인자로 넣어야 함.
	/// </summary>
	/// <param name="_size"></param>
	void MoveRear(int _size);

	/// <summary>
	/// Dequeue된 내부 버퍼의 front 위치를 옮긴다.
	/// 사이즈가 가능한 범위인지 체크하지 않으므로 실제 복사된 크기를 인자로 넣어야 함.
	/// </summary>
	/// <param name="_size"></param>
	void MoveFront(int _size);

	/// <summary>
	/// 링버퍼 초기화
	/// </summary>
	void ClearBuffer()
	{
		rear = 0;
		front = 0;
		fillSize = 0;
	}

	/// <summary>
	/// front의 포인터 반환
	/// </summary>
	/// <returns></returns>
	char* GetFrontPtr();
	/// <summary>
	/// rear의 포인터 반환
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

	// 읽기 전용
	char* buf;
	unsigned int bufSize;
	
	// 각각 다른 스레드에서 읽을 수 있음. 무효화 방지
	alignas(64) unsigned int front;
	alignas(64) unsigned int rear;

	alignas(64) int fillSize;
	alignas(64)	SRWLOCK srw;
};


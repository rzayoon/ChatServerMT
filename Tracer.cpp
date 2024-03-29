#include <Windows.h>
#include <stdio.h>
#include "Tracer.h"




const unsigned int Tracer::mask = 0xFFFF;
const unsigned int MiniTracer::mask = 0x00FF;


void Tracer::trace(char code, const long long session, const long long value, const long long value2)
{
	unsigned long long seq = InterlockedIncrement64(&pos);
	unsigned int _pos = seq & mask;

	buf[_pos].id = GetCurrentThreadId();
	buf[_pos].seq = seq;
	buf[_pos].act = code;
	buf[_pos].session = session;
	buf[_pos].info = value;
	buf[_pos].info2 = value2;
}

void Tracer::Crash()
{
	// 미사용
	trace(99, NULL, NULL); // 99 로그 밑으로는 무시
	
	int a = 0;
	// suspend other threads, and run to Write file

	FILE* fp;

	fopen_s(&fp, "debug.csv", "w");
	if (fp == nullptr)
		return;
	fprintf_s(fp, "index,thread,seq,act,l_node,r_node,cnt\n");

	fclose(fp);

	a = 1;
}

void MiniTracer::trace(char code, const unsigned long long error_code, const long long value, const long long value2)
{
	unsigned long long seq = InterlockedIncrement64(&pos);
	unsigned int _pos = seq & mask;

	buf[_pos].id = GetCurrentThreadId();
	buf[_pos].seq = seq;
	buf[_pos].act = code;
	buf[_pos].err_code = error_code;
	buf[_pos].info = value;
	buf[_pos].info2 = value2;
}

void MiniTracer::Crash()
{
	// 미사용
	trace(99, NULL, NULL); // 99 로그 밑으로는 무시

	int a = 0;
	// suspend other threads, and run to Write file

	FILE* fp;

	fopen_s(&fp, "debug.csv", "w");
	if (fp == nullptr)
		return;
	fprintf_s(fp, "index,thread,seq,act,l_node,r_node,cnt\n");

	fclose(fp);

	a = 1;
}
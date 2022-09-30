#include <Windows.h>
#include <stdio.h>
#include "Tracer.h"




const unsigned int Tracer::mask = 0xFFFF;



void Tracer::trace(char code, PVOID session, long long value, long long value2)
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
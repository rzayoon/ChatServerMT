#include <stdio.h>

#include "ProfileTls.h"


THREAD_PROFILE threadProfiles[MAX_THREAD];
alignas(64) int tpIdx = -1;
alignas(64) char end;

DWORD InitTlsIndex()
{
	DWORD idx = TlsAlloc();
	if (idx == TLS_OUT_OF_INDEXES)
	{
		wprintf(L"%d tls error\n", GetLastError());
		int* a = nullptr;
		*a = 0;
	}

	return idx;
}

static DWORD tlsIdx = InitTlsIndex();

void InitProfile(THREAD_PROFILE* tp, int profileIdx);

void ProfileBegin(const wchar_t* tag)
{
	THREAD_PROFILE* tp = (THREAD_PROFILE*)TlsGetValue(tlsIdx);
	if (tp == nullptr)
	{
		int idx = InterlockedIncrement((LONG*)&tpIdx);
		if (idx == MAX_THREAD)
		{
			wprintf(L"Profile thread not available");
			int* a = nullptr;
			*a = 0;

			return;
		}

		tp = &threadProfiles[idx];
		tp->thread_id = GetCurrentThreadId();
		InitializeSRWLock(&tp->srw);
		TlsSetValue(tlsIdx, (LPVOID)tp);
	}

	int cnt;
	int curIdx;
	bool found = false;
	stPROFILE* profiles = tp->profiles;
	AcquireSRWLockExclusive(&tp->srw);

	for (cnt = 0; cnt < MAX_PROFILE; cnt++)
	{
		if (!profiles[cnt].flag) continue;

		if (wcscmp(profiles[cnt].name, tag) == 0)
		{
			curIdx = cnt;
			found = true;
			break;
		}
	}

	if (!found)
	{
		curIdx = tp->idx++;
		wcscpy_s(profiles[curIdx].name, tag);

		InitProfile(tp, curIdx);
	}

	if (profiles[curIdx].startTime.QuadPart)
	{
		int* a = nullptr;
		*a = 0;
	}

	QueryPerformanceCounter(&profiles[curIdx].startTime);

	ReleaseSRWLockExclusive(&tp->srw);
}

void ProfileEnd(const wchar_t* tag)
{
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);

	THREAD_PROFILE* tp = (THREAD_PROFILE*)TlsGetValue(tlsIdx);
	if (tp == nullptr)
	{
		int* p = nullptr;
		*p = 0;
	}

	bool found = false;
	int cnt;
	int curIdx;
	__int64 diffTime;
	__int64 cmpTime;
	__int64 tempTime;

	stPROFILE* profiles = tp->profiles;
	AcquireSRWLockExclusive(&tp->srw);

	for (cnt = 0; cnt < MAX_PROFILE; cnt++)
	{
		if (!profiles[cnt].flag) continue;

		if (wcscmp(profiles[cnt].name, tag) == 0)
		{
			curIdx = cnt;
			found = true;
			break;
		}
	}

	if (!found)
	{
		int* a = nullptr;
		*a = 0;
	}


	diffTime = endTime.QuadPart - profiles[curIdx].startTime.QuadPart;

	profiles[curIdx].startTime.QuadPart = 0;

	cmpTime = diffTime;
	for (cnt = 0; cnt < 2; cnt++)
	{
		if (profiles[curIdx].min[cnt] > cmpTime)
		{
			tempTime = profiles[curIdx].min[cnt];
			profiles[curIdx].min[cnt] = cmpTime;
			cmpTime = tempTime;
		}
	}

	cmpTime = diffTime;
	for (cnt = 0; cnt < 2; cnt++)
	{
		if (profiles[curIdx].max[cnt] < cmpTime)
		{
			tempTime = profiles[curIdx].max[cnt];
			profiles[curIdx].max[cnt] = cmpTime;
			cmpTime = tempTime;

		}

	}

	profiles[curIdx].totalTime += diffTime;
	profiles[curIdx].call++;

	ReleaseSRWLockExclusive(&tp->srw);
}



void ProfileDataOutText(const wchar_t* fileName)
{
	static LARGE_INTEGER frq;
	if (!frq.QuadPart)
	{
		QueryPerformanceFrequency(&frq);
	}

	FILE* fp;
	errno_t errno;


	errno = _wfopen_s(&fp, fileName, L"w");
	if (fp == nullptr)
	{
		return;
	}

	const WCHAR* colName[] = { L"Thread", L"Name", L"Average", L"Min", L"Max", L"Call" };

	fwprintf_s(fp, L"-----------------------------------------------------------------------------------------------------\n\n");
	fwprintf_s(fp, L"%10s |%16s  |%12s  |%12s  |%12s  |%12s  |\n", colName[0], colName[1], colName[2], colName[3], colName[4], colName[5]);
	fwprintf_s(fp, L"-----------------------------------------------------------------------------------------------------\n");

	double average;
	double min;
	double max;
	for (int i = 0; i <= tpIdx; i++)
	{
		stPROFILE* profiles = threadProfiles[i].profiles;
		AcquireSRWLockShared(&threadProfiles[i].srw);
		for (int j = 0; j < MAX_PROFILE; j++)
		{
			if (!profiles[j].flag) continue;

			average = (double)(profiles[j].totalTime - (profiles[j].min[0] + profiles[j].min[1]) - (profiles[j].max[0] + profiles[j].max[1]))
				/ (profiles[j].call - 4) / frq.QuadPart * MEGA_ARG;
			min = (double)profiles[j].min[0] / frq.QuadPart * MEGA_ARG;
			max = (double)profiles[j].max[0] / frq.QuadPart * MEGA_ARG;


			fwprintf_s(fp, L"%10d |%16s  |%12.3fus|%12.1fus|%12.1fus|%12lld  |\n",
				threadProfiles[i].thread_id, profiles[j].name, average, min, max, profiles[j].call);
		}
		ReleaseSRWLockShared(&threadProfiles[i].srw);
		fwprintf_s(fp, L"-----------------------------------------------------------------------------------------------------\n");
	}

	fclose(fp);
}


void ProfilePrint()
{
	static LARGE_INTEGER frq;
	if (!frq.QuadPart)
	{
		QueryPerformanceFrequency(&frq);
	}



	const WCHAR* colName[] = { L"Thread", L"Name", L"Average", L"Min", L"Max", L"Call" };

	wprintf_s(L"-----------------------------------------------------------------------------------------------------\n\n");
	wprintf_s(L"%10s |%16s  |%12s  |%12s  |%12s  |%12s  |\n", colName[0], colName[1], colName[2], colName[3], colName[4], colName[5]);
	wprintf_s(L"-----------------------------------------------------------------------------------------------------\n");

	double average;
	double min;
	double max;
	for (int i = 0; i <= tpIdx; i++)
	{
		stPROFILE* profiles = threadProfiles[i].profiles;
		AcquireSRWLockShared(&threadProfiles[i].srw);
		for (int j = 0; j < MAX_PROFILE; j++)
		{
			if (!profiles[j].flag) continue;

			average = (double)(profiles[j].totalTime - (profiles[j].min[0] + profiles[j].min[1]) - (profiles[j].max[0] + profiles[j].max[1]))
				/ (profiles[j].call - 4) / frq.QuadPart * MEGA_ARG;
			min = (double)profiles[j].min[0] / frq.QuadPart * MEGA_ARG;
			max = (double)profiles[j].max[0] / frq.QuadPart * MEGA_ARG;


			wprintf_s(L"%10d |%16s  |%12.3fus|%12.1fus|%12.1fus|%12lld  |\n",
				threadProfiles[i].thread_id, profiles[j].name, average, min, max, profiles[j].call);
		}
		ReleaseSRWLockShared(&threadProfiles[i].srw);
		wprintf_s(L"-----------------------------------------------------------------------------------------------------\n");
	}

}

void ProfileReset(void)
{
	for (int i = 0; i <= tpIdx; i++)
	{
		THREAD_PROFILE* tp = &threadProfiles[i];
		stPROFILE* profiles = threadProfiles[i].profiles;
		AcquireSRWLockExclusive(&tp->srw);
		for (int j = 0; j < MAX_PROFILE; j++)
		{
			if (!profiles[j].flag) continue;

			InitProfile(tp, j);

		}
		ReleaseSRWLockExclusive(&tp->srw);
	}


}

void InitProfile(THREAD_PROFILE* tp, int profileIdx)
{
	tp->profiles[profileIdx].flag = true;

	tp->profiles[profileIdx].min[0] = LLONG_MAX;
	tp->profiles[profileIdx].min[1] = LLONG_MAX;
	tp->profiles[profileIdx].max[0] = 0;
	tp->profiles[profileIdx].max[1] = 0;

	tp->profiles[profileIdx].call = 0;
	tp->profiles[profileIdx].totalTime = 0;
	tp->profiles[profileIdx].startTime.QuadPart = 0;

	return;
}
#pragma once
#include <Windows.h>

/*
usage

CCpuUsage CPUTime(); // CPUTime(hProcess)
while(1)
{
	CPUTime.UpdateCpuTime();
	wprintf(L"Processor:%f / Process:%f \n", CPUTime.ProcessorTotal(), CPUTime.ProcessTotal());
	wprintf(L"ProcessorKernel:%f / ProcessKernel:%f \n", CPUTime.ProcessorKernel(), CPUTime.ProcessKernel());
	wprintf(L"ProcessorUser:%f / ProcessUser:%f \n", CPUTime.ProcessorUser(), CPUTime.ProcessUser());

	// Sleep(1000);


}

OR

while(1)
{
	CPUTime.Show();
	Sleep(1000);
}


*/


class CCpuUsage
{
public:
	CCpuUsage(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void UpdateCpuTime(void);

	float ProcessorTotal(void) { return _fProcessorTotal; }
	float ProcessorUser(void) { return _fProcessorUser; }
	float ProcessorKernel(void) { return _fProcessorKernel; }

	float ProcessTotal(void) { return _fProcessTotal; }
	float ProcessUser(void) { return _fProcessUser; }
	float ProcessKernel(void) { return _fProcessKernel; }

	void Show(void);


private:
	HANDLE _hProcess;
	int _iNumberOfProcessors;

	float _fProcessorTotal;
	float _fProcessorUser;
	float _fProcessorKernel;

	float _fProcessTotal;
	float _fProcessUser;
	float _fProcessKernel;

	ULARGE_INTEGER _ftProcessor_LastKernel;
	ULARGE_INTEGER _ftProcessor_LastUser;
	ULARGE_INTEGER _ftProcessor_LastIdle;

	ULARGE_INTEGER _ftProcess_LastKernel;
	ULARGE_INTEGER _ftProcess_LastUser;
	ULARGE_INTEGER _ftProcess_LastTime;
};


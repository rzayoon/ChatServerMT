
#include "CPDH.h"

#include <Psapi.h>
#include <strsafe.h>
#pragma comment(lib, "Pdh.lib")

CPDH::CPDH()
{
	

}

bool CPDH::Init()
{
	DWORD pid = GetCurrentProcessId();
	HANDLE Handle = OpenProcess(
		PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
		FALSE,
		pid /* This is the PID, you can find one from windows task manager */
	);
	
	wchar_t szProcessName[MAX_PATH] = L"\0";
	if (Handle)
	{
		GetModuleBaseName(Handle, NULL, szProcessName, MAX_PATH);
		CloseHandle(Handle);
	}
	else
	{
		return false;
	}



	int len = wcslen(szProcessName);
	szProcessName[len - 4] = L'\0';

	PdhOpenQuery(NULL, NULL, &_hQuery);

	int iCnt = 0;
	bool bErr = false;
	wchar_t* szCur = NULL;
	wchar_t* szCounters = NULL;
	wchar_t* szInterfaces = NULL;
	wchar_t szQuery[MAX_PATH];

	DWORD dwCounterSize = 0, dwInterfaceSize = 0;
	

	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters
		, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);

	szCounters = new wchar_t[dwCounterSize];
	szInterfaces = new wchar_t[dwInterfaceSize];

	if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize
		, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
	{
		delete[] szCounters;
		delete[] szInterfaces;
		return false;
	}

	iCnt = 0;
	szCur = szInterfaces;

	for (; *szCur != L'\0' && iCnt < e_PDH_ETHERNET_MAX; szCur += wcslen(szCur) + 1, iCnt++)
	{
		_EthernetStruct[iCnt]._bUse = true;
		_EthernetStruct[iCnt]._szName[0] = L'\0';

		wcscpy_s(_EthernetStruct[iCnt]._szName, szCur);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(wchar_t) * MAX_PATH, L"\\Network Interface(%s)\\Bytes Received/sec", szCur);
		PdhAddCounter(_hQuery, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(wchar_t) * MAX_PATH, L"\\Network Interface(%s)\\Bytes Sent/sec", szCur);
		PdhAddCounter(_hQuery, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes);
	}



	


	wsprintf(szQuery, L"\\Process(%s)\\Private Bytes", szProcessName);
	PdhAddCounter(_hQuery, szQuery, NULL, &_PDHprivateBytes);

	wsprintf(szQuery, L"\\Process(%s)\\Pool Nonpaged Bytes", szProcessName);
	PdhAddCounter(_hQuery, szQuery, NULL, &_PDHprocessNonPaged);

	PdhAddCounter(_hQuery, L"\\Memory\\Available MBytes", NULL, &_PDHavailMB);

	PdhAddCounter(_hQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &_PDHtotalNonPaged);


	PdhCollectQueryData(_hQuery);

	return true;
}

void CPDH::Collect()
{
	PdhCollectQueryData(_hQuery);
	PDH_FMT_COUNTERVALUE counterVal;

	PDH_STATUS Status;

	Status = PdhGetFormattedCounterValue(_PDHprivateBytes, PDH_FMT_LONG, NULL, &counterVal);
	if(Status == 0) _privateBytes = counterVal.longValue;
	Status = PdhGetFormattedCounterValue(_PDHprocessNonPaged, PDH_FMT_LONG, NULL, &counterVal);
	if (Status == 0) _processNonPaged = counterVal.longValue;
	Status = PdhGetFormattedCounterValue(_PDHavailMB, PDH_FMT_LONG, NULL, &counterVal);
	if (Status == 0) _availMB = counterVal.longValue;
	Status = PdhGetFormattedCounterValue(_PDHtotalNonPaged, PDH_FMT_LONG, NULL, &counterVal);
	if (Status == 0) _totalNonPaged = counterVal.longValue;

	_pdh_value_Network_RecvBytes = 0;
	_pdh_value_Network_SendBytes = 0;

	for (int iCnt = 0; iCnt < e_PDH_ETHERNET_MAX; iCnt++)
	{

		if (_EthernetStruct[iCnt]._bUse)
		{
			PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes,
				PDH_FMT_DOUBLE, NULL, &counterVal);
			if (Status == 0)
				_pdh_value_Network_RecvBytes += counterVal.doubleValue;
			PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes,
				PDH_FMT_DOUBLE, NULL, &counterVal);
			if (Status == 0)
				_pdh_value_Network_SendBytes += counterVal.doubleValue;

		}

	}

	return;

}

void CPDH::Show()
{
	wprintf(L"Private Bytes : %d\n", _privateBytes);
	wprintf(L"Process Non Paged : %d\n", _processNonPaged);
	wprintf(L"Total Non Paged : %d\n", _totalNonPaged);
	wprintf(L"Available MBytes : %d\n", _availMB);

	wprintf(L"Network RecvBytes : %llf\n", _pdh_value_Network_RecvBytes);
	wprintf(L"Network SendBytes : %llf\n\n", _pdh_value_Network_SendBytes);

}

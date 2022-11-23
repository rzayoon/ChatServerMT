#include <strsafe.h>
#include <time.h>

#include <direct.h>

#include "CLog.h"



wchar_t g_szLogDir[dfLOG_DIR_MAX];
int g_iLogLevel;
bool g_bLogInit;

SRWLOCK g_LogSRW;

const wchar_t* g_szLogLevel[] = { L"DEBUG", L"ERROR", L"SYSTEM" };

alignas(64) ULONG32 g_iLogCounter;

void SYSLOG_DIRECTORY(const wchar_t* dir);
void SYSLOG_LEVEL(int level);

void SYSLOG_Init(const wchar_t* dir, int level)
{
	SYSLOG_DIRECTORY(dir);
	SYSLOG_LEVEL(level);

	InitializeSRWLock(&g_LogSRW);
	g_bLogInit = true;

	return;

}

void SYSLOG_DIRECTORY(const wchar_t* dir)
{

	if (StringCchCopyW(g_szLogDir, dfLOG_DIR_MAX, dir) != S_OK)
	{
		int* a = nullptr;
		*a = 0;
	}
	
	_wmkdir(dir);

	return;
}

void SYSLOG_LEVEL(int level)
{
	if (level < enLOG_LEVEL_DEBUG || level > enLOG_LEVEL_SYSTEM)
	{
		int* a = nullptr;
		*a = 0;
	}

	g_iLogLevel = level;

	return;
}

void Log(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szStringFormat, ...)
{
	wchar_t log_buf[dfLOG_BUF_MAX];
	wchar_t log_filename[dfLOG_FILENAME_MAX];
	HRESULT hResult;
	errno_t err;

	if (g_bLogInit == false) // 초기화 여부 확인
	{
		int* a = nullptr;
		*a = 0;
	}

	if (logLevel < enLOG_LEVEL_DEBUG || logLevel > enLOG_LEVEL_SYSTEM)
	{
		int* a = nullptr;
		*a = 0;
		return;
	}


	if (g_iLogLevel <= logLevel)
	{
		time_t base_time = time(NULL);
		tm base_date_local;

		localtime_s(&base_date_local, &base_time);

		hResult = StringCchPrintfW(log_filename, dfLOG_FILENAME_MAX, L"%s/%4d%02d_%s.txt",
			g_szLogDir, base_date_local.tm_year + 1900, base_date_local.tm_mon + 1, szType);
		if (hResult != S_OK)
		{
			int* a = nullptr;
			*a = 0;
		}

		va_list va;
		va_start(va, szStringFormat);
		hResult = StringCchVPrintfW(log_buf, dfLOG_BUF_MAX, szStringFormat, va);
		va_end(va);
		if (hResult != S_OK)
		{
			int* a = nullptr;
			*a = 0;
		}

		AcquireSRWLockExclusive(&g_LogSRW);
		FILE* fp;
		err = _wfopen_s(&fp, log_filename, L"a");
		if (fp == 0)
		{
			DWORD err_code = GetLastError();
			int* a = nullptr;
			*a = 0;
			return;
		}

		fwprintf_s(fp, L"[%s] [%4d-%02d-%02d %02d:%02d:%02d / %7s / %010d] %s\n"
		, szType
		, base_date_local.tm_year + 1900
		, base_date_local.tm_mon + 1
		, base_date_local.tm_mday
		, base_date_local.tm_hour
		, base_date_local.tm_min
		, base_date_local.tm_sec
		, g_szLogLevel[logLevel]
		, g_iLogCounter++
		, log_buf);

		fclose(fp);

		ReleaseSRWLockExclusive(&g_LogSRW);

		/*AcquireSRWLockExclusive(&g_LogSRW);

		log_counter = g_iLogCounter++;

		for (index = 0; index < dfLOG_TYPE_MAX; index++)
		{
			if (g_LogTypes[index].use == false) continue;

			if (wcscmp(szType, g_LogTypes[index].type)) {
				log_type = &g_LogTypes[index];
				break;
			}
		}

		if (index == dfLOG_TYPE_MAX)
		{
			for (index = 0; index < dfLOG_TYPE_MAX; index++)
			{
				if (g_LogTypes[index].use == false)
				{
					log_type = &g_LogTypes[index];
					StringCchCopyW(log_type->type, dfLOG_TYPENAME_MAX, szType);
					InitializeSRWLock(&log_type->srw);

					log_type->use = true;
					break;
				}
			}
		}*/

	}

	return;
}

void LogHex(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szLog, BYTE* pByte, int iByteLen)
{
	wchar_t log_buf[dfLOG_BUF_MAX];
	wchar_t log_filename[dfLOG_FILENAME_MAX];
	HRESULT hResult;
	errno_t err;

	if (iByteLen * 2 >= dfLOG_BUF_MAX || iByteLen <= 0)
	{
		int* a = nullptr;
		*a = 0;
	}

	if (g_bLogInit == false) // 초기화 여부 확인
	{
		int* a = nullptr;
		*a = 0;
	}

	if (logLevel < enLOG_LEVEL_DEBUG || logLevel > enLOG_LEVEL_SYSTEM)
	{
		return;
	}
	time_t base_time = time(NULL);
	tm base_date_local;

	localtime_s(&base_date_local, &base_time);

	hResult = StringCchPrintfW(log_filename, dfLOG_FILENAME_MAX, L"%s/%4d%02d_%s.txt",
		g_szLogDir, base_date_local.tm_year + 1900, base_date_local.tm_mon + 1, szType);
	if (hResult != S_OK)
	{
		int* a = nullptr;
		*a = 0;
	}
	for (int i = 0; i < iByteLen; i++)
	{
		hResult = StringCchPrintfW(log_buf + (i * 2), 4, L"%02x", *(pByte + i));
		if (hResult != S_OK)
		{
			int* a = nullptr;
			*a = 0;
		}
	}

	AcquireSRWLockExclusive(&g_LogSRW);
	FILE* fp;
	err = _wfopen_s(&fp, log_filename, L"a");
	if (fp == 0)
	{
		int* a = nullptr;
		*a = 0;
		return;
	}

	fwprintf_s(fp, L"[%s] [%4d-%02d-%02d %02d:%02d:%02d / %7s / %010d] %s\n"
		L" : %s\n"
		, szType
		, base_date_local.tm_year + 1900
		, base_date_local.tm_mon + 1
		, base_date_local.tm_mday
		, base_date_local.tm_hour
		, base_date_local.tm_min
		, base_date_local.tm_sec
		, g_szLogLevel[logLevel]
		, g_iLogCounter++
		, szLog
		, log_buf);

	fclose(fp);

	ReleaseSRWLockExclusive(&g_LogSRW);

	return;
}
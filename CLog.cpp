#include <strsafe.h>
#include <time.h>

#include <direct.h>

#include "CLog.h"
#include "CrashDump.h"



const wchar_t* const g_szLogLevel[3] = { L"DEBUG", L"ERROR", L"SYSTEM" };

alignas(64) ULONG64 CLog::m_logCounter = 0;


void CLog::SYSLOG_Init(const wchar_t* dir, int level)
{
	SYSLOG_DIRECTORY(dir);
	SYSLOG_LEVEL(level);

	InitializeSRWLock(&logSRW);
	logInit = true;

	return;

}

void CLog::SYSLOG_DIRECTORY(const wchar_t* dir)
{

	if (StringCchCopyW(logDir, dfLOG_DIR_MAX, dir) != S_OK)
	{
		CrashDump::Crash();
	}
	
	_wmkdir(dir);

	return;
}

void CLog::SYSLOG_LEVEL(int logLevel)
{
	if (level < enLOG_LEVEL_DEBUG || level > enLOG_LEVEL_SYSTEM)
	{
		CrashDump::Crash();
	}

	level = logLevel;

	return;
}

CLog* CLog::Alloc()
{
	CLog* log = pool.Alloc();
	log->isHex = false;
	log->m_logHex[0] = L'\0';


	return log;
}

void CLog::Free(CLog* log)
{
	pool.Free(log);
}



bool CLog::Write(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szStringFormat, va_list va)
{

	HRESULT hResult;
	errno_t err;

	if (logInit == false) // 초기화 여부 확인
	{
		CrashDump::Crash();
	}

	if (logLevel < enLOG_LEVEL_DEBUG || logLevel > enLOG_LEVEL_SYSTEM)
	{
		CrashDump::Crash();
	}


	if (level <= logLevel)
	{
		time_t base_time = time(NULL);
		tm base_date_local;

		localtime_s(&base_date_local, &base_time);

		hResult = StringCchPrintfW(m_fileName, dfLOG_FILENAME_MAX, L"%s/%4d%02d_%s.txt",
			logDir, base_date_local.tm_year + 1900, base_date_local.tm_mon + 1, szType);
		if (hResult != S_OK)
		{
			CrashDump::Crash();
		}


		hResult = StringCchVPrintfW(m_logBuf, dfLOG_BUF_MAX, szStringFormat, va);
		if (hResult != S_OK)
		{
			CrashDump::Crash();
		}


		m_logNo = InterlockedIncrement(&m_logCounter);
		m_logLevel = logLevel;
		


		return true;

	}

	return false;
}

bool CLog::WriteHex(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szLog, BYTE* pByte, int iByteLen)
{

	HRESULT hResult;
	errno_t err;

	if (iByteLen * 2 >= dfLOG_BUF_MAX || iByteLen <= 0)
	{
		CrashDump::Crash();
	}

	if (logInit == false) // 초기화 여부 확인
	{
		CrashDump::Crash();
	}

	if (logLevel < enLOG_LEVEL_DEBUG || logLevel > enLOG_LEVEL_SYSTEM)
	{
		CrashDump::Crash();
	}


	if (level <= logLevel)
	{
		time_t base_time = time(NULL);
		tm base_date_local;

		localtime_s(&base_date_local, &base_time);

		hResult = StringCchPrintfW(m_fileName, dfLOG_FILENAME_MAX, L"%s/%4d%02d_%s.txt",
			logDir, base_date_local.tm_year + 1900, base_date_local.tm_mon + 1, szType);
		if (hResult != S_OK)
		{
			CrashDump::Crash();
		}

		hResult = StringCchPrintfW(m_logBuf, dfLOG_BUF_MAX, L"%s", szLog);

		for (int i = 0; i < iByteLen; i++)
		{
			hResult = StringCchPrintfW(m_logHex + (i * 2), 4, L"%02x", *(pByte + i));
			if (hResult != S_OK)
			{
				CrashDump::Crash();
			}
		}

		m_logNo = InterlockedIncrement(&m_logCounter);
		m_logLevel = logLevel;
		

		return true;
	}

	return false;
}

void CLog::WriteFile()
{
	errno_t err;

	time_t base_time = time(NULL);
	tm base_date_local;

	localtime_s(&base_date_local, &base_time);

	AcquireSRWLockExclusive(&logSRW);
	FILE* fp;
	err = _wfopen_s(&fp, m_fileName, L"a");
	if (fp == 0)
	{
		CrashDump::Crash();
		return;
	}

	fwprintf_s(fp, L"[%4d-%02d-%02d %02d:%02d:%02d / %7s / %010lld] %s\n"
		L" : %s\n"
		, base_date_local.tm_year + 1900
		, base_date_local.tm_mon + 1
		, base_date_local.tm_mday
		, base_date_local.tm_hour
		, base_date_local.tm_min
		, base_date_local.tm_sec
		, g_szLogLevel[m_logLevel]
		, m_logNo
		, m_logBuf
		, m_logHex);

	fclose(fp);

	ReleaseSRWLockExclusive(&logSRW);
}

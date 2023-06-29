#pragma once

#include <Windows.h>

#include "LockFreeQueue.h"
#include "MemoryPoolTls.h"

#define dfLOG_DIR_MAX 64
#define dfLOG_BUF_MAX 1024
#define dfLOG_FILENAME_MAX 128

#define dfLOG_TYPE_MAX 64
#define dfLOG_TYPENAME_MAX 32

enum en_LOG_LEVEL
{
	enLOG_LEVEL_DEBUG = 0,
	enLOG_LEVEL_ERROR,
	enLOG_LEVEL_SYSTEM
};


class CLog
{
	friend class CNetServer;
	friend class CLanServer;
	friend class CNetClient;
	friend class CLanClient;
	friend class MemoryPoolTls<CLog>;

private:

	CLog()
	{

	}
	~CLog()
	{

	}

	inline static wchar_t logDir[dfLOG_DIR_MAX];
	inline static int level = 0;
	inline static bool logInit = false;
	inline static SRWLOCK logSRW;

	inline static MemoryPoolTls<CLog> pool = MemoryPoolTls<CLog>(100, false);

	alignas(64) static ULONG64 m_logCounter;

	static void SYSLOG_Init(const wchar_t* dir, int level);
	static void SYSLOG_DIRECTORY(const wchar_t* dir);
	static void SYSLOG_LEVEL(int level);

	static CLog* Alloc();
	static void Free(CLog* log);

	bool Write(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szStringFormat, va_list va);
	bool WriteHex(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szLog, BYTE* pByte, int iByteLen);

	void WriteFile();

	bool isHex;

	wchar_t m_type[dfLOG_TYPE_MAX];
	wchar_t m_logBuf[dfLOG_BUF_MAX];
	wchar_t m_logHex[dfLOG_BUF_MAX];
	wchar_t m_fileName[dfLOG_FILENAME_MAX];
	en_LOG_LEVEL m_logLevel;
	
	unsigned __int64 m_logNo;

};






#pragma once

#include <Windows.h>

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



void SYSLOG_Init(const wchar_t* dir, int level);




void Log(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szStringFormat, ...);

void LogHex(const wchar_t* szType, en_LOG_LEVEL logLevel, const wchar_t* szLog, BYTE* pByte, int iByteLen);
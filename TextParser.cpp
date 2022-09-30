#include "TextParser.h"
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>


bool TextParser::LoadFile(const char* fileName)
{
	FILE* fp;
	int fileSize;
	size_t size_ret;
	fopen_s(&fp, fileName, "rb");
	if (fp == 0)
		return false;

	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);

	rewind(fp);

	_buffer = new char[fileSize];

	size_ret = fread_s(_buffer, fileSize, 1, fileSize, fp);
	if (size_ret != fileSize)
	{
		return false;
	}

	fclose(fp);

	return true;
}

bool TextParser::GetValue(const char* key, int* value)
{
	char* chpBuff, chWord[256];
	int iLength;

	_bufPointer = _buffer;

	while (GetNextWord(&chpBuff, &iLength))
	{
		memset(chWord, 0, 256);
		memcpy(chWord, chpBuff, iLength);

		if (0 == strcmp(key, chWord))
		{
			if (GetNextWord(&chpBuff, &iLength))
			{
				memset(chWord, 0, 256);
				memcpy(chWord, chpBuff, iLength);
				if (0 == strcmp(chWord, "="))
				{
					if (GetNextWord(&chpBuff, &iLength))
					{
						memset(chWord, 0, 256);
						memcpy(chWord, chpBuff, iLength);
						*value = atoi(chWord);
						return true;
					}
					return false;
				}
			}
			return false;
		}
	}
	return false;
}

bool TextParser::GetStringValue(const char* key, char* value, int value_size)
{

	char* chpBuff, chWord[256];
	int iLength;

	_bufPointer = _buffer;

	while (GetNextWord(&chpBuff, &iLength))
	{
		memset(chWord, 0, 256);
		memcpy(chWord, chpBuff, iLength);

		if (0 == strcmp(key, chWord))
		{
			if (GetNextWord(&chpBuff, &iLength))
			{
				memset(chWord, 0, 256);
				memcpy(chWord, chpBuff, iLength);
				if (0 == strcmp(chWord, "="))
				{
					if (GetStringWord(&chpBuff, &iLength))
					{
						memset(value, 0, value_size);
						memcpy(value, chpBuff, iLength);
						return true;
					}
					return false;
				}
			}
			return false;
		}
	}
	return false;
}



bool TextParser::SkipNoneCommand()
{
	while (1)
	{
		if (*_bufPointer == '}') return false;

		if (*_bufPointer == '{' || *_bufPointer == '.' || *_bufPointer == ',' ||
			*_bufPointer == 0x20 || *_bufPointer == 0x08 || *_bufPointer == 0x09 ||
			*_bufPointer == 0x0a || *_bufPointer == 0x0d)
		{
			_bufPointer++;
		}
		else if (*_bufPointer == '/' && *(_bufPointer + 1) == '*')
		{
			while (*(_bufPointer - 1) != '*' || *_bufPointer != '/')
			{
				_bufPointer++;
			}
			_bufPointer++;
		}
		else if (*_bufPointer == '/' && *(_bufPointer + 1) == '/')
		{
			while (*_bufPointer != 0x0d)
			{
				_bufPointer++;
			}
		}
		else
		{
			break;
		}

	}
	return true;

}

bool TextParser::GetNextWord(char** ppBuffer, int* pLength)
{
	if (!SkipNoneCommand()) return false;

	*pLength = 0;
	*ppBuffer = _bufPointer;

	if (*_bufPointer == '=')
	{
		*pLength = 1;
		_bufPointer++;
		return true;
	}


	while (1)
	{
		if (*_bufPointer == ' ' || *_bufPointer == '=' || *_bufPointer == 0x09
			|| *_bufPointer == 0x0a || *_bufPointer == 0x0d)
		{
			break;
		}
		_bufPointer++;
		*pLength += 1;

	}
	

	return true;


}

bool TextParser::GetStringWord(char** ppBuffer, int* pLength)
{
	if (!SkipNoneCommand()) return false;

	if (*_bufPointer != '"') return false;
	_bufPointer++;

	*pLength = 0;
	*ppBuffer = _bufPointer;

	
	while (1)
	{
		if (*_bufPointer == '"')
		{
			_bufPointer++;
			break;
		}
		_bufPointer++;
		*pLength += 1;
	}


	return true;

}
#pragma once


class TextParser
{
public:
	TextParser() : _buffer(nullptr), _bufPointer(nullptr)
	{

	}
	~TextParser()
	{
		if (_buffer != nullptr)
		{

			delete[] _buffer;

		}
	}

	bool LoadFile(const char* fileName);
	bool GetValue(const char* key, int* value);
	bool GetStringValue(const char* key, char* value, int value_size);

private:

	char* _buffer;
	char* _bufPointer;


	bool SkipNoneCommand();
	bool GetNextWord(char** ppBuffer, int* pLength);
	bool GetStringWord(char** ppBuffer, int* pLength);

};
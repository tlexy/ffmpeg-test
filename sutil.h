#pragma once

#ifndef UTIL_XX_H
#define UTIL_XX_H

#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdint>


namespace SUtil
{


	//获得当前的系统时间
	unsigned int getCurrentTime();

	std::string trim(const std::string&);

	std::string toCompactString(const std::string &str);
	void toCompactStringWith(const std::string &in_str, std::string& out_str, size_t header_len);

	size_t getFileLength(std::ifstream &_f);
	size_t readAll(std::ifstream &_ifs, char* _buff, int _buff_len);
	size_t readBinaryFile(const char* path, std::string& buf);
	size_t readTextFile(const char* path, std::string& buf);

	std::string toWinStylePath(const std::string path);
	void writeFileBinary(const char* path, const char* buf, size_t len);

	uint16_t crc16(const char *buf, int len);

	std::string hex2Str(const char* str, unsigned int len);
	std::string str2Hex(const char* str, unsigned int len);
	std::string buffer2Hex(const char* _buffer, size_t _len);
	std::string toDateTimeString(const time_t time);
	std::string toDateTimeFormat(const time_t time);

	void saveNumberToFile(int num, const char* filename);

	int64_t getTimeStampMilli();

}

#endif

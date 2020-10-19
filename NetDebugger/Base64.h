#pragma once

#include <cstdint>
#include <string>
#include <vector>


inline std::string Base64EncodeString(const void* pdata, size_t data_len)
{
	static const char baseCharTable[] =
	{
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
	};

	size_t i;
	std::string encode;
	auto data = reinterpret_cast<const uint8_t*>(pdata);
	for (i = 0; i + 3 <= data_len; i += 3)
	{
		encode.push_back(baseCharTable[data[i] >> 2]);
		encode.push_back(baseCharTable[((data[i] << 4) & 0x30) | (data[i + 1] >> 4)]);
		encode.push_back(baseCharTable[((data[i + 1] << 2) & 0x3c) | (data[i + 2] >> 6)]);
		encode.push_back(baseCharTable[data[i + 2] & 0x3f]);
	}

	if (i < data_len)
	{
		auto tail = data_len - i;
		if (tail == 1)
		{
			encode.push_back(baseCharTable[data[i] >> 2]);
			encode.push_back(baseCharTable[(data[i] << 4) & 0x30]);
			encode.push_back('=');
			encode.push_back('=');
		}
		else
		{
			encode.push_back(baseCharTable[data[i] >> 2]);
			encode.push_back(baseCharTable[((data[i] << 4) & 0x30) | (data[i + 1] >> 4)]);
			encode.push_back(baseCharTable[(data[i + 1] << 2) & 0x3c]);
			encode.push_back('=');
		}
	}
	return encode;
}

inline std::string Base64EncodeString(const std::string& data)
{
	return Base64EncodeString(data.data(), data.length());
}

inline std::string Base64EncodeString(const std::vector<uint8_t>& data)
{
	return Base64EncodeString(data.data(), data.size());
}

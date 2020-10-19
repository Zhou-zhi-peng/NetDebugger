#pragma once

#include <locale>
#include <codecvt>

enum class TextEncodeType : int
{
	ASCII = 0,
	UTF8 = 1,
	UTF16LE = 2,
	UTF32LE = 3,
	UTF16BE = 4,
	UTF32BE = 5,
	HEX = 6,
	_MAX
};

inline const wchar_t* TextEncodeTypeName(TextEncodeType type)
{
	switch (type)
	{
	case TextEncodeType::ASCII: return L"ASCII";
	case TextEncodeType::UTF8: return L"UTF8";
	case TextEncodeType::UTF16LE: return L"UTF16";
	case TextEncodeType::UTF32LE: return L"UTF32";
	case TextEncodeType::UTF16BE: return L"UTF16-BE";
	case TextEncodeType::UTF32BE: return L"UTF32-BE";
	case TextEncodeType::HEX: return L"HEX";
	}
	return L"ASCII";
}

namespace Transform
{
	inline static uint16_t swap(uint16_t value)
	{
		return (uint16_t)(((uint8_t)(value >> 8)) | ((uint16_t)(value << 8)));
	}

	inline static uint32_t swap(uint32_t value)
	{
		return (uint32_t)(
			((uint8_t)(value >> 24))
			| ((uint32_t)((value >> 8) & 0x0000FF00))
			| ((uint32_t)((value << 8) & 0x00FF0000))
			| ((uint32_t)((value << 24) & 0xFF000000)));
	}

	inline std::wstring ascii_string_to_utf16(const std::vector<uint8_t>& data)
	{
		std::wstring result;
		auto length = data.size();
		auto src = data.data();
		auto end = src + length;
		for (auto p = src; p < end; ++p)
		{
			if (*p > 127)
				result.append(1, '?');
			else
				result.append(1, *p);
		}
		return result;
	}
	inline std::wstring utf8_string_to_utf16(const std::vector<uint8_t>& data)
	{
		std::wstring result;
		auto length = data.size();
		auto src = data.data();
		auto end = src + length;
		for (auto p = src; p < end; ++p)
		{
			if (*p > 127)
				result.append(1, '?');
			else
				result.append(1, *p);
		}
		return result;
	}

	inline std::string bin_string_to_hexutf8(const std::vector<uint8_t>& data)
	{
		std::string s(data.size() << 1, 0);
		const static char kHEX_TABLE[] = "0123456789ABCDEF";
		auto end = data.data() + data.size();
		auto d = (char*)s.data();
		for (auto p = data.data(); p != end; ++p)
		{
			auto c = *p;
			*d++ = kHEX_TABLE[static_cast<unsigned char>(c >> 4)];
			*d++ = kHEX_TABLE[static_cast<unsigned char>(c & 0x0F)];
		}
		return s;
	}
	inline std::wstring bin_string_to_hexwstring_format(const std::vector<uint8_t>& data)
	{
		std::wstring s(data.size() * 3, 0);
		const static wchar_t kHEX_TABLE[] = L"0123456789ABCDEF";
		auto end = data.data() + data.size();
		auto d = (wchar_t*)s.data();
		for (auto p = data.data(); p != end; ++p)
		{
			auto c = static_cast<uint8_t>(*p);
			*d++ = kHEX_TABLE[static_cast<unsigned char>(c >> 4)];
			*d++ = kHEX_TABLE[static_cast<unsigned char>(c & 0x0F)];
			*d++ = L' ';
		}
		return s;
	}
	inline std::wstring bin_string_to_hexwstring_format(const std::wstring& data)
	{
		std::wstring s(data.size() * 3, 0);
		const static wchar_t kHEX_TABLE[] = L"0123456789ABCDEF";
		auto end = data.data() + data.size();
		auto d = (wchar_t*)s.data();
		for (auto p = data.data(); p != end; ++p)
		{
			auto c = static_cast<uint8_t>(*p);
			*d++ = kHEX_TABLE[static_cast<unsigned char>(c >> 4)];
			*d++ = kHEX_TABLE[static_cast<unsigned char>(c & 0x0F)];
			*d++ = L' ';
		}
		return s;
	}

	inline void hexutf16_to_bin_string(const std::wstring& data, std::vector<uint8_t>& result)
	{
		auto p = data.data();
		auto end = p + data.length();
		size_t i = 0;
		uint8_t d = 0;
		result.reserve(data.length() / 2);
		while (p < end)
		{
			d = 0;
			if (*p >= '0' && *p <= '9')
				d |= (*p - '0') << 4;
			else if (*p >= 'A' && *p <= 'F')
				d |= (*p - 'A' + 10) << 4;
			else if (*p >= 'a' && *p <= 'f')
				d |= (*p - 'a' + 10) << 4;
			else
			{
				++p;
				continue;
			}

			if (++p >= end)
			{
				result.push_back(d);
				break;
			}

			if (*p >= '0' && *p <= '9')
				d |= (*p - '0');
			else if (*p >= 'A' && *p <= 'F')
				d |= (*p - 'A' + 10);
			else if (*p >= 'a' && *p <= 'f')
				d |= (*p - 'a' + 10);

			result.push_back(d);
			++p;
			++i;
		}
	}

	inline std::wstring DecodeToWString(const std::vector<uint8_t>& data, TextEncodeType type)
	{
		try
		{
			switch (type)
			{
			case TextEncodeType::ASCII:
			{
				std::wstring result;
				auto length = data.size();
				auto src = data.data();
				auto end = src + length;
				for (auto p = src; p < end; ++p)
				{
					if (*p > 127)
						result.append(1, '?');
					else
						result.append(1, *p);
				}
				return result;
			}
			case TextEncodeType::UTF8:
			{
				std::wstring result;
				std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> strCnv;
				auto first = reinterpret_cast<const char*>(data.data());
				auto last = first + data.size();
				return strCnv.from_bytes(first, last);
			}
			case TextEncodeType::UTF16LE:
			{
				std::wstring result;
				result.reserve(data.size() / sizeof(wchar_t));
				auto first = reinterpret_cast<const wchar_t*>(data.data());
				auto last = first + data.size();
				result.assign(first, last);
				return result;
			}
			case TextEncodeType::UTF32LE:
			{
				std::wstring result;
				result.reserve(data.size() / sizeof(uint32_t));
				auto first = reinterpret_cast<const uint32_t*>(data.data());
				auto last = first + (data.size() / sizeof(uint32_t));
				result.assign(first, last);
				return result;
			}
			case TextEncodeType::UTF16BE:
			{
				std::wstring result;
				result.reserve(data.size() / sizeof(wchar_t));
				auto first = reinterpret_cast<const wchar_t*>(data.data());
				auto last = first + (data.size() / sizeof(wchar_t));
				while (first != last)
				{
					result.push_back((wchar_t)swap((uint16_t)(*first)));
					++first;
				}
				return result;
			}
			case TextEncodeType::UTF32BE:
			{
				std::wstring result;
				result.reserve(data.size() / sizeof(uint32_t));
				auto first = reinterpret_cast<const uint32_t*>(data.data());
				auto last = first + (data.size() / sizeof(uint32_t));
				while (first != last)
				{
					result.push_back((wchar_t)swap((uint32_t)(*first)));
					++first;
				}
				return result;
			}
			case TextEncodeType::HEX:
			{
				return bin_string_to_hexwstring_format(data);
			}
			}
		}
		catch (...)
		{

		}
		return std::wstring(data.begin(), data.end());
	}

	inline void DecodeWStringTo(const std::wstring& data, TextEncodeType type, std::vector<uint8_t>& result)
	{
		try
		{
			switch (type)
			{
			case TextEncodeType::ASCII:
			{
				auto src = data.data();
				auto end = data.data() + data.length();
				for (auto p = src; p < end; ++p)
				{
					if (*p > 127)
						result.push_back('?');
					else
						result.push_back((uint8_t)*p);
				}
				break;
			}
			case TextEncodeType::UTF8:
			{
				std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> strCnv;
				auto temp = strCnv.to_bytes(data);
				result.resize(temp.length());
				memcpy(result.data(), temp.data(), result.size());
				break;
			}
			case TextEncodeType::UTF16LE:
			{
				result.resize(data.length() * sizeof(wchar_t));
				memcpy(result.data(), data.data(), result.size() * sizeof(wchar_t));
				break;
			}
			case TextEncodeType::UTF32LE:
			{
				auto first = reinterpret_cast<const uint16_t*>(data.data());
				auto last = first + data.length();
				result.resize(data.length() * sizeof(uint32_t));
				auto p = reinterpret_cast<uint32_t*>(result.data());
				while (first != last)
				{
					*p++ = *first++;
				}
				break;
			}
			case TextEncodeType::UTF16BE:
			{
				auto first = reinterpret_cast<const uint16_t*>(data.data());
				auto last = first + data.length();
				result.resize(data.length() * sizeof(uint16_t));
				auto p = reinterpret_cast<uint16_t*>(result.data());
				while (first != last)
				{
					*p++ = swap(*first++);
				}
				break;
			}
			case TextEncodeType::UTF32BE:
			{
				auto first = reinterpret_cast<const uint16_t*>(data.data());
				auto last = first + data.length();
				result.resize(data.length() * sizeof(uint32_t));
				auto p = reinterpret_cast<uint32_t*>(result.data());
				while (first != last)
				{
					*p++ = swap(*first++);
				}
				break;
			}
			case TextEncodeType::HEX:
			{
				hexutf16_to_bin_string(data, result);
				break;
			}
			default:
				result.resize(data.size()*sizeof(wchar_t));
				memcpy(result.data(), data.data(), result.size());
				break;
			}
		}
		catch (...)
		{
			result.resize(data.size() * sizeof(wchar_t));
			memcpy(result.data(), data.data(), result.size());
		}
	}
}
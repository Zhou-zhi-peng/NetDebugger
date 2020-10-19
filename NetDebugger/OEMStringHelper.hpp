#pragma once

inline std::string WStringToString(const std::wstring& unicode)
{
	auto len = WideCharToMultiByte(CP_UTF8, 0, unicode.c_str(), (int)unicode.length(), nullptr, 0, nullptr, nullptr);
	std::string utf8;
	utf8.resize(len);
	WideCharToMultiByte(CP_UTF8, 0, unicode.c_str(), (int)unicode.length(), const_cast<char*>(utf8.data()), (int)utf8.length(), nullptr, nullptr);
	return utf8;
}

inline std::wstring StringToWString(const std::string& mstring)
{
	auto len = ::MultiByteToWideChar(CP_OEMCP, 0, mstring.data(), (int)mstring.length(), nullptr, 0);
	std::wstring wstr;
	wstr.resize(len);
	::MultiByteToWideChar(CP_OEMCP, 0, mstring.data(), (int)mstring.length(), const_cast<wchar_t*>(wstr.data()), (int)wstr.length());
	return wstr;
}

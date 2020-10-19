#pragma once
#include <memory>
#include <vector>

class LanguageData;
class LanguageService
{
public:
	LanguageService(const LanguageService&) = delete;
	LanguageService();
	~LanguageService();
public:
	void LoadData(const void* buffer, size_t size);
	bool SetLanguage(const CString& lid);
	CString GetLanguage(void);
	CString Translate(const CString& tid);
	CString Translate(const CString& tid, const CString& lid);
	std::vector<std::pair<CString, CString>> Names(void);
	void SetDefaultLID(const CString& lid) { m_DefaultLID = lid; }
	CString GetDefaultLID(void) const { return m_DefaultLID; }
private:
	CString m_CurrentLID;
	CString m_DefaultLID;
	std::unique_ptr<LanguageData> m_Data;
};


#include "pch.h"
#include <unordered_map>
#include "LanguageService.h"

struct CStringHash
{
	size_t operator() (const CString& s) const noexcept
	{
		const WCHAR* pStart = s.GetString();
		const WCHAR* pEnd = pStart +s.GetLength();
		size_t hash = 5381;
		int c;
		while (pStart < pEnd)
		{
			c = *pStart++;
			hash = ((hash << 5) + hash) + c;
		}
		return hash;
	}
};

class StringHashMap : public std::unordered_map<CString, CString, CStringHash>
{
public:
	CString title;
};

using TranslateData = std::shared_ptr<StringHashMap>;
class LanguageData
{
public:
	TranslateData Get(const CString& lid)
	{
		auto i = m_data.find(lid);
		if (i == m_data.end())
			return nullptr;
		return i->second;
	}

	void Set(const CString& lid, const CString& ltitle, const CString& tid, const CString& text)
	{
		TranslateData d;
		auto i = m_data.find(lid);
		if (i == m_data.end())
		{
			d = std::make_shared<StringHashMap>();
			d->title = ltitle;
			m_data.insert(std::make_pair(lid, d));
		}
		else
		{
			d = i->second;
		}
		
		d->insert(std::make_pair(tid, text));
	}

	std::unordered_map<CString, TranslateData, CStringHash> m_data;
	TranslateData m_current;
};


LanguageService::LanguageService():
	m_Data(new LanguageData())
{

}

LanguageService::~LanguageService()
{
	m_Data = nullptr;
}

static CString GetUserDefaultLID()
{
	TCHAR localName[255] = { 0 };
	//ResolveLocaleName(L"ja", localName, 255);
	//SetThreadLocale(LocaleNameToLCID(localName, 0));
	auto cid = GetThreadLocale();
	LCIDToLocaleName(cid, localName, 255, 0);
	return localName;
}

static const WCHAR* GetSection(CString& result, const WCHAR* input, const WCHAR* end)
{
	while (input < end)
	{
		if (*input != ']' && *input != '\n')
		{
			result.AppendChar(*input);
			++input;
		}
		else
		{
			++input;
			break;
		}
	}
	result.TrimRight();
	return input;
}

static const WCHAR* SkipComment(const WCHAR* input, const WCHAR* end)
{
	while (input < end)
	{
		if (*input == '\n')
		{
			++input;
			break;
		}
		++input;
	}
	return input;
}
static const WCHAR* GetString(CString& result, const WCHAR* input, const WCHAR* end)
{
	bool es = false;
	while (input < end)
	{
		if (es)
		{
			switch (*input)
			{
			case 'a': result.AppendChar('\a'); break;
			case 'b': result.AppendChar('\b'); break;
			case 'f': result.AppendChar('\f'); break;
			case 'n': result.AppendChar('\n'); break;
			case 'r': result.AppendChar('\r'); break;
			case 't': result.AppendChar('\t'); break;
			case 'v': result.AppendChar('\v'); break;
			case '\'': result.AppendChar('\''); break;
			case '"': result.AppendChar('"'); break;
			case '\\': result.AppendChar('\\'); break;
			case 'u': 
			{
				TCHAR tempbuf[5] = {0};
				int i = 0;
				++input;
				while (i<4 && input < end)
				{
					tempbuf[i++] = *input++;
				}
				tempbuf[4] = 0;
				auto c = (WCHAR)std::wcstoul(tempbuf, nullptr, 16);
				result.AppendChar(c);
				break;
			}
			default:
				result.AppendChar(*input);
				break;
			}
			es = false;
		}
		else if (*input == '\\')
		{
			es = true;
		}
		else
		{
			if (*input != '"')
			{
				result.AppendChar(*input);
			}
			else
			{
				break;
			}
		}
		++input;
	}
	return input;
}

static const WCHAR* GetKey(CString& result, const WCHAR* input, const WCHAR* end)
{
	if (*input == '"')
		return GetString(result, ++input, end);
	while (input < end)
	{
		if (*input != '=' && *input != '\n')
		{
			result.AppendChar(*input);
			++input;
		}
		else
		{
			break;
		}
	}
	result.TrimRight();
	return input;
}

static const WCHAR* GetValue(CString& result, const WCHAR* input, const WCHAR* end)
{
	if (*input == '"')
	{
		input = GetString(result, ++input, end);
		return SkipComment(input, end);
	}
	while (input < end)
	{
		if (*input != '\n')
		{
			result.AppendChar(*input);
			++input;
		}
		else
		{
			if (!result.IsEmpty())
			{
				if (result[result.GetLength() - 1] == '\r')
					result.Delete(result.GetLength() - 1);
			}
			++input;
			break;
		}
	}
	result.TrimRight();
	return input;
}

void LanguageService::LoadData(const void* buffer, size_t size)
{
	const WCHAR* pStart = (const WCHAR*)buffer;
	const WCHAR* pEnd = (const WCHAR*)(((const BYTE*)buffer) + size);
	CString section;
	CString key;
	CString value;
	CString langid;
	CString langtitle;
	bool valueMode = false;
	while (pStart< pEnd)
	{
		auto ch = *pStart++;
		switch (ch)
		{
		case '[':
		{
			section.Empty();
			pStart = GetSection(section, pStart, pEnd);
			auto sp = section.FindOneOf(L"|");
			if (sp > 0)
			{
				langid = section.Left(sp);
				langtitle = section.Mid(sp + 1).Trim();
			}
			else
			{
				langid = section;
				langtitle = section;
			}
		}
		break;
		case ';':
			pStart = SkipComment(pStart, pEnd);
			break;
		case '=':
			if(!key.IsEmpty())
				valueMode = true;
			else
				pStart = SkipComment(pStart, pEnd);
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\b':
		case 0xA0:
		case 0xFEFF:
			break;
		default:
			if (!valueMode)
			{
				pStart = GetKey(key, pStart > buffer ? pStart-1 : pStart, pEnd);
				if (*pStart == '\n')
				{
					key.Empty();
					++pStart;
				}
			}
			else
			{
				pStart = GetValue(value, pStart-1, pEnd);
				valueMode = false;
				m_Data->Set(langid,langtitle, key, value);
				key.Empty();
				value.Empty();
			}
			break;
		}
	}

	SetLanguage(GetUserDefaultLID());
}

bool LanguageService::SetLanguage(const CString& lid)
{
	auto l = m_Data->Get(lid);
	if (l != nullptr)
	{
		m_Data->m_current = l;
		m_CurrentLID = lid;
		return true;
	}
	return false;
}

CString LanguageService::GetLanguage(void)
{
	return m_CurrentLID;
}
CString LanguageService::Translate(const CString& tid)
{
	if(m_Data->m_current==nullptr)
		return Translate(m_DefaultLID, tid);
	auto i = m_Data->m_current->find(tid);
	if (i == m_Data->m_current->end())
		return Translate(m_DefaultLID, tid);
	return i->second;
}

CString LanguageService::Translate(const CString& tid, const CString& lid)
{
	auto l = m_Data->Get(lid);
	if (l == nullptr)
		return lid;
	auto i = l->find(tid);
	if (i == l->end())
		return tid;
	return i->second;
}

std::vector<std::pair<CString, CString>> LanguageService::Names(void)
{
	std::vector<std::pair<CString, CString>> names;
	for (auto i : m_Data->m_data)
	{
		names.push_back(std::make_pair(i.first,i.second->title));
	}
	return names;
}
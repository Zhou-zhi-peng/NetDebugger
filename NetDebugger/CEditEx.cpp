#include "pch.h"
#include "CEditEx.h"
#include "GdiplusAux.hpp"
#include "TextEncodeType.h"
#include <Imm.h>
#pragma comment(lib, "Imm32.lib")

CRichEditEx::CRichEditEx():
	m_bHEXMode(false)
{
}

CRichEditEx::~CRichEditEx()
{

}

void CRichEditEx::InitializeControl()
{
	SetAutoURLDetect(TRUE);
	SetOptions(ECOOP_OR, ECO_AUTOWORDSELECTION);
	SetTextMode(TM_RICHTEXT | TM_MULTILEVELUNDO | TM_SINGLECODEPAGE);
	SetEventMask(ENM_CHANGE);
	SetUndoLimit(64);
}

BEGIN_MESSAGE_MAP(CRichEditEx, CRichEditCtrl)
	ON_WM_CHAR()
END_MESSAGE_MAP()

void CRichEditEx::PreSubclassWindow()
{
	CRichEditCtrl::PreSubclassWindow();
	InitializeControl();
}

void CRichEditEx::ClearAll(void)
{
	SetText(L"");
}

void CRichEditEx::SetText(const std::wstring& text)
{
	SetSel(0, -1);
	ReplaceSel(text.data(), FALSE);
}

std::wstring CRichEditEx::GetContent(void)
{
	std::wstring data;
	auto len = GetWindowTextLength() + 1;
	data.resize(len);
	GetWindowText((LPTSTR)data.data(), len);
	data.resize(len-1);
	return data;
}

void CRichEditEx::AppendLabelText(const std::wstring& text)
{
	CHARRANGE start;
	CHARFORMAT cf;
	cf.cbSize = sizeof(CHARFORMAT);
	cf.dwMask = CFM_BOLD | CFM_COLOR;
	cf.dwEffects = CFE_BOLD;
	cf.crTextColor = RGB(0, 0, 255);

	SetSel(-1, -1);
	GetSel(start);
	ReplaceSel(text.data(), FALSE);
	start.cpMax = start.cpMin + (int)text.length();
	SetSel(start);
	SetSelectionCharFormat(cf);
	PostMessage(WM_VSCROLL, SB_BOTTOM, 0);
}

void CRichEditEx::AppendText(const std::wstring& text)
{
	SetSel(-1, -1);
	CHARFORMAT cf;
	cf.cbSize = sizeof(CHARFORMAT);
	cf.dwMask = CFM_BOLD | CFM_COLOR;
	cf.dwEffects = CFE_AUTOCOLOR;
	cf.crTextColor = RGB(0, 0, 0);
	SetSelectionCharFormat(cf);
	ReplaceSel(text.data(), FALSE);
}

BOOL CRichEditEx::PreTranslateMessage(MSG* pMsg)
{
	if (m_bHEXMode)
	{
		if (pMsg->message == WM_IME_COMPOSITION)
		{
			if (pMsg->lParam & GCS_RESULTSTR)
			{
				auto hIMC = ImmGetContext(pMsg->hwnd);
				if (hIMC)
				{
					auto dwSize = ImmGetCompositionString(hIMC, GCS_RESULTSTR, nullptr, 0);
					dwSize += sizeof(TCHAR);
					auto hstr = GlobalAlloc(GHND, dwSize);
					auto lpstr = (LPTSTR)GlobalLock(hstr);
					ImmGetCompositionString(hIMC, GCS_RESULTSTR, lpstr, dwSize);
					for (long i = 0; i < dwSize; ++i)
						::SendMessage(pMsg->hwnd, WM_CHAR, (WPARAM)lpstr[i], (LPARAM)0);
					ImmReleaseContext(pMsg->hwnd, hIMC);
					GlobalUnlock(hstr);
					GlobalFree(hstr);
				}
			}
			return TRUE;
		}
		else if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'V')
		{
			bool ctrl = (GetKeyState(VK_CONTROL) & 0x80)!=0;
			bool shift = (GetKeyState(VK_SHIFT) & 0x80) != 0;
			bool alt = (GetKeyState(VK_MENU) & 0x80) != 0;
			if (ctrl && (!shift) && (!alt))
			{
				if (OpenClipboard())
				{
					HANDLE hglb = nullptr;
					if (IsClipboardFormatAvailable(CF_UNICODETEXT))
					{
						hglb = GetClipboardData(CF_UNICODETEXT);
						if (hglb != nullptr)
						{
							auto lpstr = (LPCWSTR)GlobalLock(hglb);
							if (lpstr != nullptr)
							{
								auto dwSize = lstrlen(lpstr);
								for (long i = 0; i < dwSize; ++i)
									::SendMessage(pMsg->hwnd, WM_CHAR, (WPARAM)lpstr[i], (LPARAM)0);
								GlobalUnlock(hglb);
							}
						}
					}
					else if (IsClipboardFormatAvailable(CF_TEXT))
					{
						hglb = GetClipboardData(CF_TEXT);
						if (hglb != nullptr)
						{
							auto lpstr = (LPCSTR)GlobalLock(hglb);
							if (lpstr != nullptr)
							{
								auto dwSize = lstrlenA(lpstr);
								for (long i = 0; i < dwSize; ++i)
									::SendMessage(pMsg->hwnd, WM_CHAR, (WPARAM)lpstr[i], (LPARAM)0);
								GlobalUnlock(hglb);
							}
						}
					}
					else if (IsClipboardFormatAvailable(CF_OEMTEXT))
					{
						hglb = GetClipboardData(CF_OEMTEXT);
						if (hglb != nullptr)
						{
							auto lpstr = (LPCSTR)GlobalLock(hglb);
							if (lpstr != nullptr)
							{
								auto dwSize = lstrlenA(lpstr);
								for (long i = 0; i < dwSize; ++i)
									::SendMessage(pMsg->hwnd, WM_CHAR, (WPARAM)lpstr[i], (LPARAM)0);
								GlobalUnlock(hglb);
							}
						}
					}
					CloseClipboard();
				}
				return TRUE;
			}
		}
	}
	return CRichEditCtrl::PreTranslateMessage(pMsg);
}

static bool __ISHEXChar(TCHAR c, bool pure)
{
	static const TCHAR hex_chars_table[] = L"1234567890ABCDEFabcdef\r\n\t\b ";
	static const TCHAR hex_pure_chars_table[] = L"1234567890ABCDEFabcdef";
	if (pure)
	{
		for (int i = 0; i < sizeof(hex_pure_chars_table)/sizeof(TCHAR); ++i)
		{
			if (c == hex_pure_chars_table[i])
				return true;
		}
	}
	else
	{
		for (int i = 0; i < sizeof(hex_chars_table) / sizeof(TCHAR); ++i)
		{
			if (c == hex_chars_table[i])
				return true;
		}
	}
	return false;
}

static bool __ISHEXString(const std::wstring& s)
{
	for (auto c : s)
	{
		if (!__ISHEXChar(c, false))
			return false;
	}
	return true;
}

static std::wstring HexFilter(const wchar_t* str, size_t len)
{
	std::wstring hextext;
	hextext.reserve(1024);
	int sp = 0;
	auto p = str;
	auto end = p + len;
	while (p != end)
	{
		if (__ISHEXChar(*p, true))
		{
			hextext.push_back((char)std::toupper(*p));
			if (++sp == 2)
			{
				hextext.push_back(' ');
				sp = 0;
			}
		}
		++p;
	}
	return hextext;
}

void CRichEditEx::EnterHEXMode()
{
	auto content = GetContent();
	if (!__ISHEXString(content))
	{
		//if (AfxMessageBox(L"检测到非HEX字符是否，将当前字符转换为 HEX 编码？\r\n如果选择【是】将逐个字符转换为16进制编码，【否】则清除不是16进制的字符。", MB_YESNO) == IDYES)
		{
			SetText(Transform::bin_string_to_hexwstring_format(content));
		}
		//else
		{
			//SetText(HexFilter(content.data(), content.length()));
		}
	}
	m_bHEXMode = true;
}

void CRichEditEx::ExitHEXMode()
{
	m_bHEXMode = false;
}


void CRichEditEx::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (m_bHEXMode)
	{
		if(nChar =='\b')
			CRichEditCtrl::OnChar(nChar, nRepCnt, nFlags);
		else if (__ISHEXChar((TCHAR)nChar, true))
		{
			bool back = false;
			bool processed = false;
			TCHAR charBuffer[] = { 0,0,0,0,0,0,0,0 };
			LPCTSTR p = nullptr;
			CHARRANGE sel;
			CString prefixText;
			CString suffixText;
			charBuffer[1] = (TCHAR)std::toupper(nChar);
			GetSel(sel);
			if (sel.cpMin >= 2)
			{
				GetTextRange(sel.cpMin - 2, sel.cpMin, prefixText);
				GetTextRange(sel.cpMin, sel.cpMin + 2, suffixText);
			}
			else if(sel.cpMin == 0)
			{
				GetTextRange(sel.cpMin, sel.cpMin + 2, suffixText);
			}
			else if (sel.cpMin == 1)
			{
				GetTextRange(sel.cpMin - 1, sel.cpMin, prefixText);
				GetTextRange(sel.cpMin, sel.cpMin + 2, suffixText);
			}

			if (prefixText.GetLength() > 0 && suffixText.GetLength() > 0)
			{
				if (prefixText.GetLength() == 1 && __ISHEXChar(prefixText[0], true) && __ISHEXChar(suffixText[0], true))
				{
					charBuffer[2] = ' ';
					charBuffer[3] = '0';
					p = charBuffer + 1;
					back = true;
					processed = true;
				}
				else if (prefixText.GetLength() == 2 && __ISHEXChar(prefixText[1], true) && __ISHEXChar(suffixText[0], true))
				{
					charBuffer[2] = ' ';
					charBuffer[3] = '0';
					p = charBuffer + 1;
					back = true;
					processed = true;
				}
			}

			if(!processed)
			{
				if (prefixText.GetLength() > 0)
				{
					if ((prefixText.GetLength() > 1) && __ISHEXChar(prefixText[0], true) && __ISHEXChar(prefixText[1], true))
					{
						charBuffer[0] = ' ';
						p = charBuffer;
					}
					else
					{
						p = charBuffer + 1;
					}
				}

				if (suffixText.GetLength() > 0)
				{
					if ((suffixText.GetLength() > 1) && __ISHEXChar(suffixText[0], true) && __ISHEXChar(suffixText[1], true))
					{
						charBuffer[2] = ' ';
						back = true;
					}

					if (p == nullptr)
					{
						p = charBuffer + 1;
					}
				}
				else if (p == nullptr)
				{
					p = charBuffer + 1;
				}
			}

			ReplaceSel(p, TRUE);
			if (back)
			{
				sel.cpMin += 1;
				sel.cpMax = sel.cpMin;
				SetSel(sel);
			}
		}
	}
	else
		CRichEditCtrl::OnChar(nChar, nRepCnt, nFlags);
}

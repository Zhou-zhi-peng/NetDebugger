#pragma once
#include <afxwin.h>
class CRichEditEx : public CRichEditCtrl
{
public:
	CRichEditEx();
	virtual ~CRichEditEx();
public:
	void AppendLabelText(const std::wstring& text);
	void AppendText(const std::wstring& text);
	void SetText(const std::wstring& text);
	std::wstring GetContent(void);
	void ClearAll(void);
	void EnterHEXMode();
	void ExitHEXMode();
private:
	void InitializeControl();
protected:
	virtual void PreSubclassWindow();
private:
	bool m_bHEXMode;
protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
};


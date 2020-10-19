#pragma once
#include <afxwin.h>
class CPlaceholderEdit :
	public CEdit
{
public:
	void Placeholder(const CString& placeholder) { m_Placeholder = placeholder; }
	CString Placeholder() const { return m_Placeholder; }
private:
	CString m_Placeholder;
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
};


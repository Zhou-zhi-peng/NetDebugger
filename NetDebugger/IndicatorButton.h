#pragma once
#include <afxwin.h>
class CIndicatorButton :
	public CButton
{
public:
	void SetActive(bool active)
	{
		if (m_Active != active)
		{
			m_Active = active;
			Invalidate();
		}
	}
private:
	bool m_Active = false;
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnNcPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	virtual void DrawItem(LPDRAWITEMSTRUCT /*lpDrawItemStruct*/);
	virtual void PreSubclassWindow();
};


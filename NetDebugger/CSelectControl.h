#pragma once
#include <afxwin.h>
#include <vector>

class CSelectControl :
	public CWnd
{
public:
	CSelectControl();
	virtual ~CSelectControl();
public:
	void AddButton(CString text, UINT value);
	bool RemoveButtonAt(int index);
	void RemoveAllButton();

	UINT GetValue() const;
	void SetValue(UINT value);
private:
	struct ButtonItem
	{
		CString text;
		UINT value;
		CRect rect;
	};
	std::vector<ButtonItem> m_Buttons;
	int m_SelectedIndex;
	int m_HoverIndex;
	bool m_Pressed;
public:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseHover(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
};


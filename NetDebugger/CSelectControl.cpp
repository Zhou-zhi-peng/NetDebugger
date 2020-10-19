#include "pch.h"
#include "CSelectControl.h"
#include "UserWMDefine.h"

BEGIN_MESSAGE_MAP(CSelectControl, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEHOVER()
	ON_WM_MOUSELEAVE()
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

static BOOL RegisterWindowClass(void)
{
	LPCWSTR className = L"SELECT_BUTTON_GROUP";
	WNDCLASS windowclass;
	HINSTANCE hInstance = AfxGetInstanceHandle();

	if (!(::GetClassInfo(hInstance, className, &windowclass)))
	{
		windowclass.style = CS_DBLCLKS;
		windowclass.lpfnWndProc = ::DefWindowProc;
		windowclass.cbClsExtra = windowclass.cbWndExtra = 0;
		windowclass.hInstance = hInstance;
		windowclass.hIcon = NULL;
		windowclass.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		windowclass.hbrBackground = ::GetSysColorBrush(COLOR_WINDOW);
		windowclass.lpszMenuName = NULL;
		windowclass.lpszClassName = className;
		if (!AfxRegisterClass(&windowclass))
		{
			AfxThrowResourceException();
			return FALSE;
		}
	}
	return TRUE;
}

CSelectControl::CSelectControl()
{
	RegisterWindowClass();
	m_HoverIndex = -1;
	m_SelectedIndex = -1;
	m_Pressed = false;
}

CSelectControl::~CSelectControl()
{

}

void CSelectControl::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(client);
	CFont* font = GetTopLevelParent()->GetFont();
	int offset = client.left;
	auto save = dc.SaveDC();
	dc.SelectObject(*font);
	int i = 0;
	dc.SetBkMode(TRANSPARENT);
	dc.FillSolidRect(client, GetSysColor(COLOR_BTNHIGHLIGHT));
	for (auto& item : m_Buttons)
	{
		const auto& text = item.text;
		CRect rc(client);
		
		dc.DrawText(text, rc, DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
		rc.left = offset;
		rc.right += offset + 15;
		rc.top = client.top;
		rc.bottom = client.bottom;
		item.rect = rc;
		if (i == m_SelectedIndex)
		{
			dc.SetTextColor(GetSysColor(COLOR_HIGHLIGHTTEXT)); 
			dc.FillSolidRect(rc, GetSysColor(COLOR_HIGHLIGHT));
			dc.DrawText(text, rc, DT_LEFT | DT_VCENTER | DT_CENTER | DT_SINGLELINE);
			if (m_Pressed)
				dc.InvertRect(rc);
		}
		else if (i == m_HoverIndex)
		{
			dc.SetTextColor(GetSysColor(COLOR_HIGHLIGHTTEXT));
			dc.FillSolidRect(rc, GetSysColor(COLOR_MENUHILIGHT));
			dc.DrawText(text, rc, DT_LEFT | DT_VCENTER | DT_CENTER | DT_SINGLELINE);
			if (m_Pressed)
				dc.InvertRect(rc);
		}
		else
		{
			dc.SetTextColor(GetSysColor(COLOR_BTNTEXT));
			dc.FillSolidRect(rc, GetSysColor(COLOR_BTNHIGHLIGHT));
			dc.DrawText(text, rc, DT_LEFT | DT_VCENTER | DT_CENTER | DT_SINGLELINE);
		}
		
		offset = rc.right;
		++i;
	}
	dc.RestoreDC(save);
}


BOOL CSelectControl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}


void CSelectControl::AddButton(CString text, UINT value)
{
	ButtonItem item;
	item.text = text;
	item.value = value;
	m_Buttons.push_back(item);
}


bool CSelectControl::RemoveButtonAt(int index)
{
	if (index < 0 || index > (int)m_Buttons.size())
		return false;
	m_Buttons.erase(m_Buttons.begin() + index);
	Invalidate();
	return true;
}


void CSelectControl::RemoveAllButton()
{
	if (!m_Buttons.empty())
	{
		m_Buttons.clear();
		Invalidate();
	}
}

UINT CSelectControl::GetValue() const
{
	if (m_SelectedIndex < 0 || m_SelectedIndex > (int)m_Buttons.size())
		return 0;
	return m_Buttons[m_SelectedIndex].value;
}

void CSelectControl::SetValue(UINT value)
{
	int i = 0;
	for (auto& item : m_Buttons)
	{
		if (item.value == value)
			break;
		++i;
	}
	if (i >= (int)m_Buttons.size())
		m_SelectedIndex = -1;
	else
		m_SelectedIndex = i;
	Invalidate();
}

void CSelectControl::OnMouseMove(UINT nFlags, CPoint point)
{
	TRACKMOUSEEVENT  tme = { 0 };
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_HOVER | TME_LEAVE;
	tme.dwHoverTime = 50;
	tme.hwndTrack = this->m_hWnd;
	TrackMouseEvent(&tme);
	int i = 0;
	for (auto& item : m_Buttons)
	{
		if (item.rect.PtInRect(point))
		{
			if (m_HoverIndex != i)
			{
				m_HoverIndex = i;
				Invalidate();
			}
			return;
		}
		++i;
	}
	if (m_HoverIndex != -1)
	{
		m_HoverIndex = -1;
		Invalidate();
	}
	CWnd::OnMouseMove(nFlags, point);
}


void CSelectControl::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_Pressed = true;
	if (m_HoverIndex != m_SelectedIndex)
	{
		m_SelectedIndex = m_HoverIndex;
		::SendMessage(::GetParent(this->GetSafeHwnd()), kWM_SELECT_BUTTON_CHANGED, this->GetDlgCtrlID(), m_SelectedIndex);
	}
	Invalidate();
}

void CSelectControl::OnMouseHover(UINT nFlags, CPoint point)
{
	CWnd::OnMouseHover(nFlags, point);
}


void CSelectControl::OnMouseLeave()
{
	m_Pressed = false;
	m_HoverIndex = -1;
	Invalidate();
	CWnd::OnMouseLeave();
}


void CSelectControl::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_Pressed = false;
	Invalidate();
	CWnd::OnLButtonUp(nFlags, point);
}

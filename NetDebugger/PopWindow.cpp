#include "pch.h"
#include "PopWindow.h"
#include "Resource.h"
#include <mutex>
#include <list>
#include <algorithm>
#include "GdiplusAux.hpp"

constexpr int kPOPWIN_ICON_SIZE = 86;
constexpr int kBUTTON_HEIGHT = 28;
constexpr UINT kWM_CREATE_POP_WINDOW = WM_USER + 8;
constexpr UINT kWM_DELETE_POP_WINDOW = kWM_CREATE_POP_WINDOW + 1;
constexpr UINT kWM_UPDATE_POP_WINDOW = kWM_DELETE_POP_WINDOW + 1;

Gdiplus::PrivateFontCollection* g_pfc = nullptr;
static BOOL Initialize_Control(void)
{
	LPCWSTR className = L"NOTIFICATION_POPWINDOW";
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
		windowclass.hbrBackground = (HBRUSH)::GetStockObject(NULL_BRUSH); //::GetSysColorBrush(COLOR_WINDOW);
		windowclass.lpszMenuName = NULL;
		windowclass.lpszClassName = className;
		if (!AfxRegisterClass(&windowclass))
		{
			AfxThrowResourceException();
			return FALSE;
		}

		HRSRC res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_POPWND), RT_FONT);
		if (res)
		{
			HGLOBAL mem = LoadResource(hInstance, res);
			void *data = LockResource(mem);
			DWORD len = SizeofResource(hInstance, res);
			g_pfc = new Gdiplus::PrivateFontCollection();
			g_pfc->AddMemoryFont(data, len);
			UnlockResource(mem);
			FreeResource(mem);
		}
		else
		{
			AfxThrowResourceException();
			return FALSE;
		}
	}
	return TRUE;
}

class PopWindowImpl : public CWnd
{
	friend class PopWindowData;
protected:
	PopWindowImpl(
		const CString& title, 
		const CString& body, 
		int duration, 
		PopWindow::WindowType type, 
		std::shared_ptr<PopWindowListener> listener,
		const CString& buttons,
		int id);

	virtual ~PopWindowImpl(void);
	void CreateHWND(void);
	void UpdateWindow(const CString& title, const CString& body, int duration, PopWindow::WindowType type);
private:
	void UpdateIcon();
	bool CalcWindowSize();
	void DrawButtons(Gdiplus::Graphics& graphics);
	void SplitButtonString(const CString& string);
private:
	struct ButtonState
	{
		ButtonState(const CString& text,int id):
			Hover(false),
			Pressed(false),
			ButtonRect(0,0,0,0),
			Text(text),
			Id(id)
		{
		}
		bool Hover;
		bool Pressed;
		CRect ButtonRect;
		CString Text;
		int Id;
	};

	CString m_Title;
	CString m_Body;
	int m_Duration;
	int m_IconOrientation;
	CRect m_Rect;
	COLORREF m_BKColor;
	COLORREF m_TextColor;
	COLORREF m_IconColor;
	PopWindow::WindowType m_Type;
	std::shared_ptr<PopWindowListener> m_Listener;
	std::vector<ButtonState> m_Buttons;
	int m_Id;
	CPoint m_DownPoint;
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
public:
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnMouseHover(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnClose();
};

BEGIN_MESSAGE_MAP(PopWindowImpl, CWnd)
	ON_WM_ACTIVATE()
	ON_WM_SHOWWINDOW()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_MOUSEHOVER()
	ON_WM_MOUSELEAVE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_ERASEBKGND()
	ON_WM_CLOSE()
END_MESSAGE_MAP()

class PopWindowData
{
private:
	struct PopWindowParam
	{
		int id;
		CString title;
		CString body;
		PopWindow::WindowType type;
		int duration;
		std::shared_ptr<PopWindowListener> listener;
		CString buttons;
	};
public:
	PopWindowData() = default;
	~PopWindowData() = default;
public:
	void AddWindow(PopWindowImpl* wnd)
	{
		std::unique_lock<std::mutex> clk(m_Mutex);
		for (auto w : m_VisibleWindowList)
		{
			if (w == wnd)
				return;
		}

		m_VisibleWindowList.push_back(wnd);
		UpdatePosition();
	}
	void RemoveWindow(PopWindowImpl* wnd,bool destory)
	{
		{
			std::unique_lock<std::mutex> clk(m_Mutex);
			auto it = std::find(m_VisibleWindowList.begin(), m_VisibleWindowList.end(), wnd);
			if (it != m_VisibleWindowList.end())
			{
				m_VisibleWindowList.erase(it);
				UpdatePosition();
			}
		}
		if (destory)
			ClosePopWindow(wnd->m_Id);
	}

	void AdjustWindowsPosition(void)
	{
		std::unique_lock<std::mutex> clk(m_Mutex);
		UpdatePosition();
	}

	int CreatePopWindow(
		const CString& title, 
		const CString& body, 
		PopWindow::WindowType type, 
		int duration,
		std::shared_ptr<PopWindowListener> listener,
		const CString* buttons)
	{
		PopWindowParam* param = new PopWindowParam();
		auto id = NewWindowId();
		param->id = id;
		param->title = title;
		param->body = body;
		param->type = type;
		param->duration = duration;
		param->listener = listener;
		if (buttons != nullptr)
			param->buttons = *buttons;
		SendWindowMessage(kWM_CREATE_POP_WINDOW, param);
		return id;
	}

	void DeletePopWindow(int wid)
	{
		SendWindowMessage(kWM_DELETE_POP_WINDOW, (void*)((size_t)wid));
	}

	void UpdatePopWindow(int wid, const CString& title, const CString& body, PopWindow::WindowType type, int duration)
	{
		PopWindowParam* param = new PopWindowParam();
		param->id = wid;
		param->title = title;
		param->body = body;
		param->type = type;
		param->duration = duration;
		param->listener = nullptr;
		param->buttons.Empty();

		SendWindowMessage(kWM_UPDATE_POP_WINDOW, param);
	}

	static PopWindowData* GetData()
	{
		static PopWindowData g_Data;
		if (g_Data.m_ParentWnd == nullptr)
		{
			g_Data.BindParentWnd(AfxGetApp()->GetMainWnd());
		}
		return &g_Data;
	}
private:
	int NewWindowId(void)
	{
		std::unique_lock<std::mutex> clk(m_Mutex);
		do
		{
			if (m_NextWindowID == INT_MAX)
				m_NextWindowID = 0;
			if (m_WindowMap.find(m_NextWindowID) == m_WindowMap.end())
				break;
			++m_NextWindowID;
		} while (true);
		auto id = m_NextWindowID++;
		m_WindowMap.insert(std::make_pair(id, nullptr));
		return id;
	}
	void NewPopWindow(const PopWindowParam& cp)
	{
		std::shared_ptr<PopWindowImpl> pop(
			new PopWindowImpl(cp.title, cp.body, cp.duration, cp.type, cp.listener, cp.buttons, cp.id), 
			[](PopWindowImpl* win) { delete win; }
		);

		{
			std::unique_lock<std::mutex> clk(m_Mutex);
			m_WindowMap[cp.id] = pop;
		}
		pop->CreateHWND();
	}
	void ChangePopWindow(const PopWindowParam& cp)
	{
		std::shared_ptr<PopWindowImpl> pop;
		{
			std::unique_lock<std::mutex> clk(m_Mutex);
			auto it = m_WindowMap.find(cp.id);
			if (it != m_WindowMap.end())
			{
				if (it->second != nullptr)
					pop = it->second;
			}
		}
		if (pop != nullptr)
		{
			pop->UpdateWindow(cp.title, cp.body, cp.duration, cp.type);
			pop->Invalidate();
		}
	}

	void ClosePopWindow(int wid)
	{
		std::shared_ptr<PopWindowImpl> pop;
		{
			std::unique_lock<std::mutex> clk(m_Mutex);
			auto it = m_WindowMap.find(wid);
			if (it != m_WindowMap.end())
			{
				if (it->second != nullptr)
					pop = it->second;
				m_WindowMap.erase(it);
			}
		}
		if (pop != nullptr)
			pop->DestroyWindow();
	}
	void UpdatePosition()
	{
		if (m_VisibleWindowList.empty())
			return;
		auto first = *(m_VisibleWindowList.begin());
		CRect firstRect;
		CRect parentRect;
		first->GetWindowRect(&firstRect);
		if (m_ParentWnd == nullptr)
		{
			GetWindowRect(::GetDesktopWindow(), &firstRect);
		}
		else
		{
			m_ParentWnd->GetWindowRect(&parentRect);
		}
		auto x = (parentRect.Width() - firstRect.Width()) / 2 + parentRect.left;
		auto y = parentRect.top + 8;
		auto w = firstRect.Width();
		auto h = firstRect.Height();
		firstRect.SetRect(x, y, x + w, y + h);
		::SetWindowPos(
			first->GetSafeHwnd(), 
			nullptr, 
			firstRect.left, firstRect.top, firstRect.Width(), firstRect.Height(), 
			SWP_NOZORDER | SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
		y = firstRect.bottom + 6;
		for (auto wnd : m_VisibleWindowList)
		{
			if (wnd == first)
				continue;

			CRect rcWnd;
			wnd->GetWindowRect(&rcWnd);
			x = (firstRect.Width() - rcWnd.Width()) / 2 + firstRect.left;
			rcWnd.SetRect(x, y, x + rcWnd.Width(), y + rcWnd.Height());
			::SetWindowPos(
				wnd->GetSafeHwnd(),
				nullptr,
				rcWnd.left, rcWnd.top, rcWnd.Width(), rcWnd.Height(), 
				SWP_NOZORDER | SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
			y = rcWnd.bottom + 6;
		}
	}

	void BindParentWnd(CWnd* parent)
	{
		if (parent == nullptr)
			return;
		m_OldParentWndProc = (WNDPROC)SetWindowLongPtr(parent->GetSafeHwnd(), GWLP_WNDPROC, (LONG_PTR)&HookWndProc);
		m_ParentWnd = parent;
	}

	void SendWindowMessage(UINT msgId, void* pParam)
	{
		auto wParam = (WPARAM)pParam;
		auto lParam = (~wParam) ^ 0xFE;
		m_ParentWnd->PostMessage(msgId, wParam, lParam);
	}

	static bool CheckMessageParam(WPARAM wParam, LPARAM lParam)
	{
		return (lParam == ((~wParam) ^ 0xFE));
	}

	/*
	拦截主窗口消息
	*/
	static LRESULT WINAPI HookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		auto gdata = GetData();
		auto oldWndProc = gdata->m_OldParentWndProc;
		if (oldWndProc == HookWndProc || oldWndProc == nullptr)
			oldWndProc = ::DefWindowProc;

		if (message == WM_MOVE || message == WM_SIZE)
		{
			std::unique_lock<std::mutex> clk(GetData()->m_Mutex);
			gdata->UpdatePosition();
		}
		else if (message == WM_DESTROY)
		{
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)(oldWndProc));
			gdata->m_OldParentWndProc = nullptr;
			gdata->m_ParentWnd = nullptr;
		}
		else if (message == kWM_CREATE_POP_WINDOW && CheckMessageParam(wParam,lParam))
		{
			auto cp = reinterpret_cast<PopWindowParam*>(wParam);
			if (cp != nullptr)
			{
				gdata->NewPopWindow(*cp);
				delete cp;
			}
			return 0;
		}
		else if (message == kWM_UPDATE_POP_WINDOW && CheckMessageParam(wParam, lParam))
		{
			auto cp = reinterpret_cast<PopWindowParam*>(wParam);
			if (cp != nullptr)
			{
				gdata->ChangePopWindow(*cp);
				delete cp;
			}
			return 0;
		}
		else if (message == kWM_DELETE_POP_WINDOW && CheckMessageParam(wParam, lParam))
		{
			auto wid = static_cast<int>(wParam);
			gdata->ClosePopWindow(wid);
			return 0;
		}
		else if (message == WM_ACTIVATE) 
		{
			/* 
			fix: 当程序从后台切换到前台（激活）时，如果此时焦点在 CRichEditCtrl(非只读)控件中，将出现界面卡现象，原因未知。
			复现：
			OS: Window 10（2004 19041.508）
			操作步骤:	（1）运行程序；
						（2）点击发送框使其拥有输入焦点；
						（3）切换到其它程序界面；
						（4）切换回程序；
						（5）界面出现未响应，问题复现。
			临时解决方案：
			拦截WM_ACTIVATE消息，不调用原窗口消息处理过程，直接调用默认窗口消息处理过程(DefWindowProc)
			最终解决方案：无
			*/
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return oldWndProc(hWnd, message, wParam, lParam);
	};
private:
	WNDPROC m_OldParentWndProc = nullptr;
	CWnd* m_ParentWnd = nullptr;
	std::list<PopWindowImpl*> m_VisibleWindowList;
	std::map<int,std::shared_ptr<PopWindowImpl>> m_WindowMap;
	std::mutex m_Mutex;
	int m_NextWindowID = 0;
};

static void DrawFontIcon(Gdiplus::Graphics& graphics, UINT id, const CRect& rect, COLORREF color, int nOrientation = 0)
{
	auto ch = static_cast<TCHAR>(id);
	auto state = graphics.Save();
	Gdiplus::FontFamily fontFamily(L"mfc-popwnd", g_pfc);
	Gdiplus::Font font(&fontFamily, (Gdiplus::REAL)rect.Height(), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::RectF rcText((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
	Gdiplus::Color colorText;
	colorText.SetFromCOLORREF(color);
	Gdiplus::SolidBrush brush(colorText);

	Gdiplus::PointF pt((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top);
	graphics.MeasureString(&ch, 1, &font, pt, &rcText);
	auto x = (rect.Width() - rcText.Width) / 2;
	//auto y = (rect.Height() - rcText.Height) / 2;
	pt.X += x;
	//pt.Y -= y;
	//graphics.TranslateTransform(pt.X, pt.Y);
	//graphics.RotateTransform(5*nOrientation);

	Gdiplus::Matrix matrix;
	graphics.GetTransform(&matrix);
	auto temp = rect.CenterPoint();
	Gdiplus::PointF cen((Gdiplus::REAL)temp.x, (Gdiplus::REAL)temp.y);

	matrix.RotateAt((Gdiplus::REAL)(5 * nOrientation), cen);
	graphics.SetTransform(&matrix);

	graphics.DrawString(&ch, 1, &font, pt, &brush);
	graphics.Restore(state);
}

static void DrawTitleFontText(Gdiplus::Graphics& graphics, const CString& str, const CRect& rect, int height, COLORREF color)
{
	Gdiplus::Color gdiColor;
	gdiColor.SetFromCOLORREF(color);
	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font font(fontFamily.get(), (Gdiplus::REAL)height, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::RectF rcText((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
	Gdiplus::SolidBrush brush(gdiColor);
	Gdiplus::StringFormat sf;
	sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
	sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisWord);
	graphics.DrawString(str.GetString(), str.GetLength(), &font, rcText, &sf, &brush);
}

static void DrawBodyFontText(Gdiplus::Graphics& graphics, const CString& str, CRect& rect, int height, COLORREF color, bool measure = false)
{
	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font font(fontFamily.get(), (Gdiplus::REAL)height, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::RectF rcText((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
	if (measure)
	{
		Gdiplus::RectF newSizeText;
		rcText.Height = 0;
		newSizeText.Width = rcText.Width;
		graphics.MeasureString(str.GetString(), str.GetLength(), &font, rcText, &newSizeText);

		rect.top = (int)newSizeText.GetTop();
		rect.left = (int)newSizeText.GetLeft();
		rect.right = (int)newSizeText.GetRight();
		rect.bottom = (int)newSizeText.GetBottom();
	}
	else
	{
		Gdiplus::Color gdiColor;
		gdiColor.SetFromCOLORREF(color);
		Gdiplus::SolidBrush brush(gdiColor);
		Gdiplus::StringFormat sf;
		graphics.DrawString(str.GetString(), str.GetLength(), &font, rcText, &sf, &brush);
	}
}

int PopWindow::Show(
	const CString& title, 
	const CString& body, 
	PopWindow::WindowType type, 
	int duration,
	std::shared_ptr<PopWindowListener> listener,
	const CString* buttons)
{
	return PopWindowData::GetData()->CreatePopWindow(title, body, type, duration, listener, buttons);
}

void PopWindow::Update(int windowId, const CString& title, const CString& body, PopWindow::WindowType type, int duration)
{
	PopWindowData::GetData()->UpdatePopWindow(windowId, title, body, type, duration);
}
void PopWindow::Close(int windowId)
{
	PopWindowData::GetData()->DeletePopWindow(windowId);
}

void PopWindowImpl::SplitButtonString(const CString& string)
{
	CString strTemp;
	TCHAR c;
	for (int i = 0; i < string.GetLength(); i++)
	{
		c = string[i];
		if (c == L'|')
		{
			if (strTemp.GetLength() > 0)
			{
				m_Buttons.push_back(ButtonState(strTemp, (int)m_Buttons.size()));
			}
			strTemp = L"";
		}
		else
		{
			strTemp.AppendChar(c);
		}
	}
	if (strTemp.GetLength() > 0)
	{
		m_Buttons.push_back(ButtonState(strTemp, (int)m_Buttons.size()));
	}
}


PopWindowImpl::PopWindowImpl(
	const CString& title, 
	const CString& body, 
	int duration,
	PopWindow::WindowType type,
	std::shared_ptr<PopWindowListener> listener,
	const CString& buttons,
	int id)
{
	Initialize_Control();

	CRect rect(0, 0, 420, kPOPWIN_ICON_SIZE);
	m_Rect = rect;
	m_Id = id;
	if (type == PopWindow::MNONE)
		type = PopWindow::MINFO;
	m_Listener = listener;
	m_Buttons.push_back(ButtonState(L"X", PopWindow::CLOSE_REASON_CLOSECMD));
	SplitButtonString(buttons);
	UpdateWindow(title, body, duration, type);
	m_DownPoint.SetPoint(-1, -1);
}

PopWindowImpl::~PopWindowImpl(void)
{
	PopWindowData::GetData()->RemoveWindow(this, true);
}

void PopWindowImpl::CreateHWND(void)
{
	if (m_Listener && m_Listener->OnCreate)
	{
		m_Listener->OnCreate(m_Title, m_Body, m_Type, m_Duration);
	}

	CreateEx(WS_EX_LAYERED, _T("NOTIFICATION_POPWINDOW"), m_Title, WS_POPUP, m_Rect, AfxGetMainWnd(), 0);
	::SetWindowPos(
		GetSafeHwnd(),
		nullptr, 
		0, 0, 0, 0, 
		SWP_NOZORDER | SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	if (m_Duration != 0)
		SetTimer(0, m_Duration, nullptr);
	if (m_Type == PopWindow::MLOADING)
		SetTimer(1, 30, nullptr);

	SetLayeredWindowAttributes(0, 220, LWA_ALPHA);
	CalcWindowSize();
	ShowWindow(SW_SHOWNOACTIVATE);
}

bool PopWindowImpl::CalcWindowSize()
{
	if (this->GetSafeHwnd() != nullptr)
	{
		CRect autoSize;
		CClientDC tempDC(this);
		Gdiplus::Graphics graphics(tempDC);
		autoSize = m_Rect;
		autoSize.left += m_Rect.Height();
		autoSize.top += 30;
		autoSize.right -= 6;
		DrawBodyFontText(graphics, m_Body, autoSize, 16, 0, true);
		m_Rect.bottom = autoSize.bottom + 6;
		if (m_Rect.Height() < kPOPWIN_ICON_SIZE)
			m_Rect.bottom = m_Rect.top + kPOPWIN_ICON_SIZE;
		if (m_Buttons.size() > 1)
		{
			m_Rect.bottom += kBUTTON_HEIGHT + 6;
		}
		::SetWindowPos(
			GetSafeHwnd(), 
			nullptr, 
			0, 0, m_Rect.Width(), m_Rect.Height(), 
			SWP_NOZORDER | SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
		return true;
	}
	return false;
}

void PopWindowImpl::UpdateWindow(const CString& title, const CString& body, int duration, PopWindow::WindowType type)
{
	m_Title = title;
	if (duration != m_Duration || (type!=m_Type && type!= PopWindow::MNONE))
	{
		m_Duration = duration;
		if (this->GetSafeHwnd() != nullptr)
		{
			KillTimer(0);
			KillTimer(1);
			if (m_Duration != 0)
				SetTimer(0, m_Duration, nullptr);
			if (type == PopWindow::MLOADING)
				SetTimer(1, 30, nullptr);
		}
	}

	if (type != PopWindow::MNONE)
	{
		switch (type)
		{
		case PopWindow::MINFO:
			m_IconColor = RGB(119, 124, 137);
			m_BKColor = RGB(69, 74, 86);
			m_TextColor = RGB(255, 255, 255);
			break;
		case PopWindow::MQUESTION:
			m_IconColor = RGB(108, 199, 87);
			m_BKColor = RGB(8, 99, 0);
			m_TextColor = RGB(255, 255, 255);
			break;
		case PopWindow::MOK:
			m_IconColor = RGB(96, 254, 134);
			m_BKColor = RGB(0, 154, 34);
			m_TextColor = RGB(255, 255, 255);
			break;
		case PopWindow::MLOADING:
			m_IconColor = RGB(83, 200, 255);
			m_BKColor = RGB(0, 100, 193);
			m_TextColor = RGB(255, 255, 255);
			break;
		case PopWindow::MERROR:
			m_IconColor = RGB(255, 136, 135);
			m_BKColor = RGB(205, 36, 35);
			m_TextColor = RGB(255, 255, 255);
			break;
		case PopWindow::MWARNING:
			m_IconColor = RGB(255, 208, 50);
			m_BKColor = RGB(226, 128, 0);
			m_TextColor = RGB(255, 255, 255);
			break;
		}
		m_Type = type;
		m_IconOrientation = 0;
	}

	if (body != m_Body)
	{
		m_Body = body;
		if (CalcWindowSize())
		{
			//PopWindowData::GetData()->AdjustWindowsPosition();
		}
	}
	if (m_Listener && m_Listener->OnUpdate)
	{
		m_Listener->OnUpdate(m_Title, m_Body, m_Type, m_Duration);
	}
}
void PopWindowImpl::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	CWnd::OnActivate(nState, pWndOther, bMinimized);
}

void PopWindowImpl::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (bShow)
	{
		PopWindowData::GetData()->AddWindow(this);
	}
	else
	{
		PopWindowData::GetData()->RemoveWindow(this, false);
	}
	CWnd::OnShowWindow(bShow, nStatus);
}

void PopWindowImpl::DrawButtons(Gdiplus::Graphics& graphics)
{
	Gdiplus::REAL x = m_Rect.right - 6.0f;
	Gdiplus::REAL y = m_Rect.bottom - kBUTTON_HEIGHT - 6.0f;
	Gdiplus::RectF buttonRect;
	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font font(fontFamily.get(), 16.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::Color gdiColor;
	gdiColor.SetFromCOLORREF(m_TextColor);
	Gdiplus::SolidBrush brush(gdiColor);
	Gdiplus::SolidBrush bkbrush(Gdiplus::Color(80, 0, 0, 0));
	Gdiplus::SolidBrush hoverBrush(Gdiplus::Color(30, 255, 255, 255));
	Gdiplus::SolidBrush pressedBrush(Gdiplus::Color(128, 0, 0, 0));
	Gdiplus::Pen pen(Gdiplus::Color(80, 255, 255, 255), 1);
	Gdiplus::StringFormat sf;
	for (auto& btn : m_Buttons)
	{
		if (btn.Id == PopWindow::CLOSE_REASON_CLOSECMD)
			continue;

		const auto& btnText = btn.Text;
		Gdiplus::RectF bTextBox;
		buttonRect.X = x;
		buttonRect.Y = y;
		buttonRect.Width = 0;
		buttonRect.Height = (Gdiplus::REAL)kBUTTON_HEIGHT;
		bTextBox.Height = buttonRect.Height;
		graphics.MeasureString(btnText, btnText.GetLength(), &font, buttonRect, &bTextBox);
		bTextBox.Width += 6;
		buttonRect.X = x - bTextBox.Width;
		buttonRect.Width = bTextBox.Width;

		sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
		sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
		if (btn.Pressed)
		{
			graphics.FillRectangle(&pressedBrush, buttonRect);
		}
		else if (btn.Hover)
		{
			graphics.FillRectangle(&hoverBrush, buttonRect);
		}
		else
		{
			graphics.FillRectangle(&bkbrush, buttonRect);
		}
		
		graphics.DrawString(btnText, btnText.GetLength(), &font, buttonRect, &sf, &brush);
		graphics.DrawRectangle(&pen, buttonRect);
		btn.ButtonRect.left = (int)buttonRect.X;
		btn.ButtonRect.top = (int)buttonRect.Y;
		btn.ButtonRect.right = (int)buttonRect.GetRight();
		btn.ButtonRect.bottom = (int)buttonRect.GetBottom();
		x = buttonRect.X - 6;
	}
}

void PopWindowImpl::OnPaint()
{
	CPaintDC pdc(this); 
	CMemDC memDC(pdc, this);
	auto& dc = memDC.GetDC();
	Gdiplus::Graphics graphics(dc);
	Gdiplus::Color bkColor;
	bkColor.SetFromCOLORREF(m_BKColor);
	graphics.Clear(bkColor);

	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);
	auto rect = m_Rect;
	rect.right = rect.left + kPOPWIN_ICON_SIZE;
	rect.bottom = rect.top + kPOPWIN_ICON_SIZE;
	rect.InflateRect(-rect.Width() / 6, -rect.Height() / 6);
	DrawFontIcon(graphics, m_Type, rect, m_IconColor, m_IconOrientation);

	rect = m_Rect;
	rect.left += kPOPWIN_ICON_SIZE;
	rect.top += 6;
	rect.bottom = rect.top + 24;
	rect.right -= 36;
	DrawTitleFontText(graphics, m_Title, rect, 18, m_TextColor);

	rect = m_Rect;
	rect.left += kPOPWIN_ICON_SIZE;
	rect.top += 30;
	rect.right -= 6;
	DrawBodyFontText(graphics, m_Body, rect, 16, m_TextColor, false);
	
	if (m_Buttons.size() > 1)
		DrawButtons(graphics);

	rect = m_Rect;
	rect.left = rect.right - 32;
	rect.bottom = rect.top + 32;
	rect.InflateRect(-6, -6);
	auto closeButtonColor = m_TextColor;
	if (m_Buttons[0].Hover)
		closeButtonColor = RGB(33, 150, 243);
	DrawFontIcon(graphics, 0xE967, rect, closeButtonColor);
	m_Buttons[0].ButtonRect = rect;
	if (m_Buttons[0].Pressed)
		dc.InvertRect(rect);
}

void PopWindowImpl::OnTimer(UINT_PTR nIDEvent)
{
	CWnd::OnTimer(nIDEvent);
	if (nIDEvent == 0)
	{
		if (m_Listener && m_Listener->OnClosing)
		{
			if (!m_Listener->OnClosing(PopWindow::CLOSE_REASON_TIMEOUT))
			{
				KillTimer(0);
				return;
			}
		}
		this->DestroyWindow();
	}
	else if (nIDEvent == 1)
		this->UpdateIcon();
}

void PopWindowImpl::UpdateIcon()
{
	m_IconOrientation++;
	Invalidate();
}
void PopWindowImpl::OnDestroy()
{
	if (m_Listener && m_Listener->OnClosed)
	{
		m_Listener->OnClosed();
	}
	PopWindowData::GetData()->RemoveWindow(this, false);
	CWnd::OnDestroy();
	PopWindowData::GetData()->RemoveWindow(this, true);
}

void PopWindowImpl::OnMouseHover(UINT nFlags, CPoint point)
{
	CWnd::OnMouseHover(nFlags, point);
}

void PopWindowImpl::OnMouseLeave()
{
	for (auto& btn : m_Buttons)
	{
		btn.Hover = false;
		btn.Pressed = false;
	}
	CWnd::OnMouseLeave();
	Invalidate();
}

void PopWindowImpl::OnMouseMove(UINT nFlags, CPoint point)
{
	TRACKMOUSEEVENT  tme = { 0 };
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_HOVER | TME_LEAVE;
	tme.dwHoverTime = 50;
	tme.hwndTrack = this->m_hWnd;
	TrackMouseEvent(&tme);

	bool changed = false;
	for (auto& btn : m_Buttons)
	{
		bool hover = btn.ButtonRect.PtInRect(point);
		if (btn.Hover != hover)
		{
			btn.Hover = hover;
			changed = true;
		}
	}

	if (changed)
		Invalidate();
	else if (nFlags & MK_LBUTTON)
	{
		auto a = m_DownPoint.x - point.x;
		auto b = m_DownPoint.y - point.y;
		auto q = std::sqrt(a*a + b * b);
		if (q > 10)
		{
			PopWindowData::GetData()->RemoveWindow(this, false);
			PostMessage(WM_NCLBUTTONDOWN,
				HTCAPTION,
				MAKELPARAM(point.x, point.y));
		}
	}
}

void PopWindowImpl::OnLButtonDown(UINT nFlags, CPoint point)
{
	bool changed = false;
	for (auto& btn : m_Buttons)
	{
		bool pressed = btn.ButtonRect.PtInRect(point);
		if (btn.Pressed != pressed)
		{
			btn.Pressed = pressed;
			changed = true;
		}
	}

	if (changed)
	{
		Invalidate();
	}
	else
	{
		m_DownPoint = point;
	}
	SetFocus();
	CWnd::OnLButtonDown(nFlags, point);
}

void PopWindowImpl::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_DownPoint.SetPoint(-1, -1);
	for (auto& btn : m_Buttons)
	{
		if (btn.ButtonRect.PtInRect(point) && btn.Pressed)
		{
			btn.Pressed = false;
			Invalidate();
			if (m_Listener && m_Listener->OnClosing)
			{
				if (!m_Listener->OnClosing(btn.Id))
				{
					return;
				}
			}
			this->DestroyWindow();
			return;
		}
		else if(btn.Pressed)
		{
			btn.Pressed = false;
			Invalidate();
		}
	}
	CWnd::OnLButtonUp(nFlags, point);
}

BOOL PopWindowImpl::OnEraseBkgnd(CDC* pDC)
{
	return FALSE;
}

void PopWindowImpl::OnClose()
{
	if (m_Listener && m_Listener->OnClosing)
	{
		if (!m_Listener->OnClosing(PopWindow::CLOSE_REASON_CLOSECMD))
		{
			return;
		}
	}
	this->DestroyWindow();
}

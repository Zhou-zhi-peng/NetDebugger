#include "pch.h"
#include "NetDebugger.h"
#include "CRealTimeStatusCtrl.h"
#include "GdiplusAux.hpp"
constexpr int kXSTEP_LENGTH = 10;

BEGIN_MESSAGE_MAP(CRealTimeStatusCtrl, CWnd)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
END_MESSAGE_MAP()

CRealTimeStatusCtrl::CRealTimeStatusCtrl():
	m_connected(false),
	m_readBytes(0),
	m_writeBytes(0),
	m_MaxReadSpeed(0),
	m_MaxWriteSpeed(0),
	m_AveReadSpeed(0),
	m_AveWriteSpeed(0)
{

}

CRealTimeStatusCtrl::~CRealTimeStatusCtrl()
{

}

void CRealTimeStatusCtrl::AddSpeedPoint(uint64_t readspeed, uint64_t writespeed)
{
	CRect rcClient;
	GetClientRect(&rcClient);
	auto count = (rcClient.Width() / kXSTEP_LENGTH) + 2;
	uint64_t sum = 0;
	if (count <= (int)m_ReadSpeeds.size() && (!m_ReadSpeeds.empty()))
	{
		auto removed = m_ReadSpeeds[0];
		m_ReadSpeeds.erase(m_ReadSpeeds.begin());
		if (removed >= m_MaxReadSpeed)
		{
			m_MaxReadSpeed = 0;
			for (auto t : m_ReadSpeeds)
			{
				if (t > m_MaxReadSpeed)
					m_MaxReadSpeed = t;
			}
		}
	}
	m_ReadSpeeds.push_back(readspeed);
	for (auto t : m_ReadSpeeds)
	{
		sum += t;
	}
	m_AveReadSpeed = sum / m_ReadSpeeds.size();
	if (readspeed > m_MaxReadSpeed)
		m_MaxReadSpeed = readspeed;

	if (count <= (int)m_WriteSpeeds.size() && (!m_WriteSpeeds.empty()))
	{
		auto removed = m_WriteSpeeds[0];
		m_WriteSpeeds.erase(m_WriteSpeeds.begin());
		if (removed >= m_MaxWriteSpeed)
		{
			m_MaxWriteSpeed = 0;
			for (auto t : m_WriteSpeeds)
			{
				if (t > m_MaxWriteSpeed)
					m_MaxWriteSpeed = t;
			}
		}
	}
	m_WriteSpeeds.push_back(writespeed);
	sum = 0;
	for (auto t : m_WriteSpeeds)
	{
		sum += t;
	}
	m_AveWriteSpeed = sum / m_WriteSpeeds.size();
	if (writespeed > m_MaxWriteSpeed)
		m_MaxWriteSpeed = writespeed;

	Invalidate();
}

void CRealTimeStatusCtrl::UpdateStatistics(bool connected, uint64_t readBytes, uint64_t writeBytes)
{
	auto usedTime = std::chrono::high_resolution_clock::now() - m_lastUpdateTime;
	auto readSpeed = static_cast<uint64_t>((readBytes - m_readBytes) / (usedTime.count() / 1000000000.0));
	auto writeSpeed = static_cast<uint64_t>((writeBytes - m_writeBytes) / (usedTime.count() / 1000000000.0));
	m_lastUpdateTime = std::chrono::high_resolution_clock::now();

	m_connected = connected;
	m_readBytes = readBytes;
	m_writeBytes = writeBytes;
	AddSpeedPoint(readSpeed, writeSpeed);
	RedrawWindow();
}

BOOL CRealTimeStatusCtrl::OnEraseBkgnd(CDC* pDC)
{
	return FALSE;
}

int CRealTimeStatusCtrl::DrawStatisicsString(Gdiplus::Graphics& graphics,const Gdiplus::RectF& clientRect)
{
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);

	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font font(fontFamily.get(), 16.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	CString statisticsRXC;
	CString statisticsTXC;
	CString statisticsRXS;
	CString statisticsTXS;

	statisticsRXC.Format(LSTEXT(MAINWND.STATISTICSCTRL.RECV.BYTES), std::to_wstring(m_readBytes).c_str());
	statisticsTXC.Format(LSTEXT(MAINWND.STATISTICSCTRL.SEND.BYTES), std::to_wstring(m_writeBytes).c_str());
	statisticsRXS.Format(LSTEXT(MAINWND.STATISTICSCTRL.RECV.SPEED), GetReadSpeedString().GetString());
	statisticsTXS.Format(LSTEXT(MAINWND.STATISTICSCTRL.SEND.SPEED), GetWriteSpeedString().GetString());

	Gdiplus::StringFormat sf;
	sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
	sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
	sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisWord);
	Gdiplus::Pen textBrPen(Gdiplus::Color(128, 0, 0, 0), 0);

	auto textRectRXC = clientRect;
	textRectRXC.Inflate(-6.0f, -6.0f);
	textRectRXC.Height = (Gdiplus::REAL)(MeasureStringHeight(graphics, statisticsRXC, font, (int)textRectRXC.Width));

	auto textRectTXC = clientRect;
	textRectTXC.Inflate(-6.0f, -6.0f);
	textRectTXC.Y = textRectRXC.GetBottom();
	textRectTXC.Height = (Gdiplus::REAL)(MeasureStringHeight(graphics, statisticsTXC, font, (int)textRectTXC.Width));

	auto textRectRXS = clientRect;
	textRectRXS.Inflate(-6.0f, -6.0f);
	textRectRXS.Y = textRectTXC.GetBottom();
	textRectRXS.Height = (Gdiplus::REAL)(MeasureStringHeight(graphics, statisticsRXS, font, (int)textRectRXS.Width));

	auto textRectTXS = clientRect;
	textRectTXS.Inflate(-6.0f, -6.0f);
	textRectTXS.Y = textRectRXS.GetBottom();
	textRectTXS.Height = (Gdiplus::REAL)(MeasureStringHeight(graphics, statisticsTXS, font, (int)textRectTXS.Width) + 3);

	auto bottom = textRectTXS.GetBottom();
	auto textBKRect = clientRect;
	textBKRect.Height = bottom - textBKRect.GetTop();

	graphics.DrawLine(&textBrPen, textBKRect.X, textBKRect.GetBottom(), textBKRect.GetRight(), textBKRect.GetBottom());

	Gdiplus::SolidBrush textBrush(Gdiplus::Color::White);

	textBrush.SetColor(Gdiplus::Color::Black);
	textRectRXC.Offset(1.0f, 1.0f);
	graphics.DrawString(statisticsRXC.GetString(), statisticsRXC.GetLength(), &font, textRectRXC, &sf, &textBrush);
	textBrush.SetColor(Gdiplus::Color::White);
	textRectRXC.Offset(-1.0f, -1.0f);
	graphics.DrawString(statisticsRXC.GetString(), statisticsRXC.GetLength(), &font, textRectRXC, &sf, &textBrush);

	textBrush.SetColor(Gdiplus::Color::Black);
	textRectTXC.Offset(1.0f, 1.0f);
	graphics.DrawString(statisticsTXC.GetString(), statisticsTXC.GetLength(), &font, textRectTXC, &sf, &textBrush);
	textBrush.SetColor(Gdiplus::Color::White);
	textRectTXC.Offset(-1.0f, -1.0f);
	graphics.DrawString(statisticsTXC.GetString(), statisticsTXC.GetLength(), &font, textRectTXC, &sf, &textBrush);

	textBrush.SetColor(Gdiplus::Color::Black);
	textRectRXS.Offset(1.0f, 1.0f);
	graphics.DrawString(statisticsRXS.GetString(), statisticsRXS.GetLength(), &font, textRectRXS, &sf, &textBrush);
	textBrush.SetColor(Gdiplus::Color(204, 255, 153));
	textRectRXS.Offset(-1.0f, -1.0f);
	graphics.DrawString(statisticsRXS.GetString(), statisticsRXS.GetLength(), &font, textRectRXS, &sf, &textBrush);

	textBrush.SetColor(Gdiplus::Color::Black);
	textRectTXS.Offset(1.0f, 1.0f);
	graphics.DrawString(statisticsTXS.GetString(), statisticsTXS.GetLength(), &font, textRectTXS, &sf, &textBrush);
	textBrush.SetColor(Gdiplus::Color(255, 102, 102));
	textRectTXS.Offset(-1.0f, -1.0f);
	graphics.DrawString(statisticsTXS.GetString(), statisticsTXS.GetLength(), &font, textRectTXS, &sf, &textBrush);

	return (int)bottom;
}

void CRealTimeStatusCtrl::DrawSpeedCurve(Gdiplus::Graphics& graphics, const Gdiplus::RectF& clientRect)
{
	Gdiplus::Pen linePen(Gdiplus::Color(204, 255, 153), 1);
	Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(50, 0, 0, 0));
	Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255));

	Gdiplus::GraphicsPath path;
	std::vector<Gdiplus::Point> points;
	int x = 0;
	int h = (int)(clientRect.Height);
	int b = (int)(clientRect.GetBottom());
	uint64_t maxSpeed = (m_MaxReadSpeed > m_MaxWriteSpeed) ? m_MaxReadSpeed : m_MaxWriteSpeed;
	uint64_t maxValue = (maxSpeed + maxSpeed / 4);
	if (maxValue == 0)
		maxValue = 64;
	double fy=0;
	for (auto t : m_ReadSpeeds)
	{
		fy = (double)t;
		fy *= h;
		fy /= maxValue;
		auto y = b - static_cast<int>(fy);
		points.push_back(Gdiplus::Point(x, y));
		x += kXSTEP_LENGTH;
	}
	path.AddCurve(points.data(), (int)points.size());
	points.clear();
	points.push_back(Gdiplus::Point(x - kXSTEP_LENGTH, b));
	points.push_back(Gdiplus::Point(0, b));
	path.AddLines(points.data(), (int)points.size());
	path.CloseFigure();

	graphics.FillPath(&shadowBrush, &path);
	graphics.DrawPath(&linePen, &path);

	linePen.SetColor(Gdiplus::Color(255, 102, 102));
	shadowBrush.SetColor(Gdiplus::Color(50, 0, 0, 0));
	textBrush.SetColor(Gdiplus::Color(255, 255, 255));

	path.Reset();
	points.clear();
	x = 0;
	fy=0;
	for (auto t : m_WriteSpeeds)
	{
		fy = (double)t;
		fy *= h;
		fy /= maxValue;
		auto y = b - static_cast<int>(fy);
		points.push_back(Gdiplus::Point(x, y));
		x += kXSTEP_LENGTH;
	}
	path.AddCurve(points.data(), (int)points.size());
	points.clear();
	points.push_back(Gdiplus::Point(x - kXSTEP_LENGTH, b));
	points.push_back(Gdiplus::Point(0, b));
	path.AddLines(points.data(), (int)points.size());
	path.CloseFigure();

	graphics.FillPath(&shadowBrush, &path);
	graphics.DrawPath(&linePen, &path);
}

void CRealTimeStatusCtrl::DrawDisconnectedStatus(Gdiplus::Graphics& graphics, const Gdiplus::RectF& clientRect)
{
	CString str = L"未连接";
	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font font(fontFamily.get(), 22.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255));
	Gdiplus::StringFormat sf;
	sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisWord);
	auto textRect = clientRect;
	graphics.DrawString(str.GetString(), str.GetLength(), &font, textRect, &sf, &brush);
	brush.SetColor(Gdiplus::Color(128, 128, 128));
	textRect.Offset(-1.0f, -1.0f);
	graphics.DrawString(str.GetString(), str.GetLength(), &font, textRect, &sf, &brush);
}

void CRealTimeStatusCtrl::OnPaint()
{
	CPaintDC pdc(this);
	CMemDC memDC(pdc, this);
	auto& dc = memDC.GetDC();
	Gdiplus::Graphics graphics(dc);
	CRect rcClient;
	GetClientRect(&rcClient);
	Gdiplus::RectF gdiRect((Gdiplus::REAL)rcClient.left, (Gdiplus::REAL)rcClient.top, (Gdiplus::REAL)rcClient.Width(), (Gdiplus::REAL)rcClient.Height());

	if (m_connected)
	{
		graphics.Clear(Gdiplus::Color(0, 102, 153));
	}
	else
	{
		graphics.Clear(Gdiplus::Color(204, 204, 204));
	}
	Gdiplus::Pen brPen(Gdiplus::Color(50, 0, 0, 0), 1);
	brPen.SetAlignment(Gdiplus::PenAlignment::PenAlignmentCenter);
	gdiRect.Width -= 1;
	gdiRect.Height -= 1;
	graphics.DrawRectangle(&brPen, gdiRect);

	auto y = DrawStatisicsString(graphics, gdiRect);
	gdiRect.Y = (Gdiplus::REAL)y;
	gdiRect.Height -= y;
	DrawSpeedCurve(graphics, gdiRect);
	if (!m_connected)
		DrawDisconnectedStatus(graphics, gdiRect);
}

static CString HumanReadableSize(uint64_t size)
{
	static const LPCTSTR units[] = { L"B/s", L"KB/s", L"MB/s", L"GB/s", L"TB/s", L"PB/s" };
	double mod = 1024.0;
	double fsize = (double)size;
	int i = 0;
	while (fsize >= mod)
	{
		fsize /= mod;
		i++;
	}
	CString result;
	result.Format(L"%.2lf %s", fsize, units[i]);
	return result;
}

CString CRealTimeStatusCtrl::GetReadSpeedString(void) const
{
	if (m_ReadSpeeds.empty())
		return HumanReadableSize(0);
	return HumanReadableSize(m_AveReadSpeed);
}
CString CRealTimeStatusCtrl::GetWriteSpeedString(void) const
{
	if (m_WriteSpeeds.empty())
		return HumanReadableSize(0);
	auto speed = m_WriteSpeeds.back();
	return HumanReadableSize(m_AveWriteSpeed);
}
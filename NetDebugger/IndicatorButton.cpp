#include "pch.h"
#include "IndicatorButton.h"
#include "GdiplusAux.hpp"

BEGIN_MESSAGE_MAP(CIndicatorButton, CButton)
	ON_WM_PAINT()
	ON_WM_NCPAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


void CIndicatorButton::OnPaint()
{
	CButton::OnPaint();
}


void CIndicatorButton::OnNcPaint()
{
}


BOOL CIndicatorButton::OnEraseBkgnd(CDC* pDC)
{
	return FALSE;
}


void CIndicatorButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (lpDrawItemStruct->CtlType != ODT_BUTTON)
		return;

	CDC pdc;
	pdc.Attach(lpDrawItemStruct->hDC);
	CMemDC memDC(pdc, this);
	auto& dc = memDC.GetDC();
	Gdiplus::Graphics graphics(dc);
	CRect rcClient = lpDrawItemStruct->rcItem;
	Gdiplus::RectF gdiRect((Gdiplus::REAL)rcClient.left, (Gdiplus::REAL)rcClient.top, (Gdiplus::REAL)rcClient.Width(), (Gdiplus::REAL)rcClient.Height());

	if (lpDrawItemStruct->itemState & ODS_SELECTED)
		graphics.Clear(Gdiplus::Color(0, 100, 146));
	else if((lpDrawItemStruct->itemState & ODS_DISABLED) || (lpDrawItemStruct->itemState & ODS_GRAYED))
		graphics.Clear(Gdiplus::Color(204, 204, 204));
	else if ((lpDrawItemStruct->itemState & ODS_HOTLIGHT) || (lpDrawItemStruct->itemState & ODS_INACTIVE) || (lpDrawItemStruct->itemState & ODS_FOCUS))
		graphics.Clear(Gdiplus::Color(50, 200, 196));
	else
	{
		if(m_Active)
			graphics.Clear(Gdiplus::Color(0, 190, 100));
		else
			graphics.Clear(Gdiplus::Color(0, 140, 186));
	}

	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);

	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font font(fontFamily.get(), 18.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	CString text;
	GetWindowText(text);

	Gdiplus::StringFormat sf;
	sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisWord);
	Gdiplus::Pen textBrPen(Gdiplus::Color(60, 0, 0, 0), 3);
	Gdiplus::SolidBrush textBrush(Gdiplus::Color::White);
	Gdiplus::SolidBrush textSBrush(Gdiplus::Color::Black);
	if (m_Active)
	{
		textBrPen.SetColor(Gdiplus::Color(80, 255, 255, 255));
		textBrPen.SetWidth(5);
		textBrush.SetColor(Gdiplus::Color(234, 0, 55));
	}
	textBrPen.SetAlignment(Gdiplus::PenAlignment::PenAlignmentCenter);
	gdiRect.Width -= 1;
	gdiRect.Height -= 1;
	graphics.DrawRectangle(&textBrPen, gdiRect);

	gdiRect.Offset(1.0f, 1.0f);
	graphics.DrawString(text.GetString(), text.GetLength(), &font, gdiRect, &sf, &textSBrush);
	gdiRect.Offset(-1.0f, -1.0f);
	graphics.DrawString(text.GetString(), text.GetLength(), &font, gdiRect, &sf, &textBrush);

	
}


void CIndicatorButton::PreSubclassWindow()
{
	CButton::PreSubclassWindow();
	ModifyStyle(0, BS_OWNERDRAW);
}

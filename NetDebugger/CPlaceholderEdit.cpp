#include "pch.h"
#include "CPlaceholderEdit.h"
#include "GdiplusAux.hpp"

BEGIN_MESSAGE_MAP(CPlaceholderEdit, CEdit)
	ON_WM_PAINT()
END_MESSAGE_MAP()


void CPlaceholderEdit::OnPaint()
{
	if (IsWindowEnabled() && GetWindowTextLength() <= 0 && this->GetFocus()!=this)
	{
		CPaintDC dc(this);
		Gdiplus::Graphics graphics(dc);
		CRect rect;
		graphics.Clear(Gdiplus::Color::White);
		GetClientRect(&rect);
		Gdiplus::RectF rcItem((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
		graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
		graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);
		Gdiplus::StringFormat sf;
		sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
		sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
		sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisCharacter);
		

		auto fontFamily = CreateUIFontFamily();
		Gdiplus::SolidBrush textBrush(Gdiplus::Color(130, 0, 0, 0));
		Gdiplus::Font fontLabel(fontFamily.get(), 12.0f, Gdiplus::FontStyleItalic, Gdiplus::UnitPixel);
		graphics.DrawString(m_Placeholder, m_Placeholder.GetLength(), &fontLabel, rcItem, &sf, &textBrush);
	}
	else
	{
		CEdit::OnPaint();
	}
}

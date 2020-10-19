#pragma once

inline Gdiplus::REAL MeasureStringHeight(Gdiplus::Graphics& graphics, const CString& str, const Gdiplus::Font& font, int width)
{
	Gdiplus::RectF newSizeText;
	auto h = font.GetHeight(&graphics);
	Gdiplus::RectF rcText(0.0f, 0.0f, (Gdiplus::REAL)width, h);
	rcText.Height = 0;
	newSizeText.Width = rcText.Width;
	graphics.MeasureString(str.GetString(), str.GetLength(), &font, rcText, &newSizeText);
	return newSizeText.Height;
}

inline std::shared_ptr<Gdiplus::FontFamily> CreateUIFontFamily()
{
	std::shared_ptr<Gdiplus::FontFamily> fontFamily(new Gdiplus::FontFamily(L"Microsoft YaHei"));
	if (fontFamily->GetLastStatus() != Gdiplus::Status::Ok)
		fontFamily.reset(new Gdiplus::FontFamily(L"Arial"));
	if (fontFamily->GetLastStatus() != Gdiplus::Status::Ok)
		fontFamily.reset(new Gdiplus::FontFamily(L"ËÎÌו"));
	if (fontFamily->GetLastStatus() != Gdiplus::Status::Ok)
	{
		LOGFONT logfont;
		auto hFont = GetStockObject(DEFAULT_GUI_FONT);
		::GetObject(hFont, sizeof(LOGFONT), &logfont);
		fontFamily.reset(new Gdiplus::FontFamily(logfont.lfFaceName));
	}

	if (fontFamily->GetLastStatus() != Gdiplus::Status::Ok)
		return nullptr;
	return fontFamily;
}
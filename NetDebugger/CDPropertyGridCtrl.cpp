#include "pch.h"
#include "NetDebugger.h"
#include "CDPropertyGridCtrl.h"
#include "UserWMDefine.h"
#include "GdiplusAux.hpp"

static void splitTitle(const CString& pdTitle, CString& title, CString& desc)
{
	if (!pdTitle.IsEmpty())
	{
		auto idx = pdTitle.Find(L"\r\n");
		if (idx >= 0)
		{
			title = pdTitle.Mid(0, idx);
			desc = pdTitle.Mid(idx + 2);
		}
		else
		{
			title = pdTitle;
			desc = title;
		}
	}
}

static CString GetOptionTitle(const IDevice::PropertyDescription::EnumOptions& options, const std::wstring& value)
{
	for (auto opt : options)
	{
		if (opt.first == value)
		{
			return LSVT(opt.second.data());
		}
	}
	return CString(value.data());
}

static CString GetOptionValue(const IDevice::PropertyDescription::EnumOptions& options, const CString& title)
{
	for (auto opt : options)
	{
		if (LSVT(opt.second.data()) == title)
		{
			return opt.first.data();
		}
	}
	return title;
}

static bool HasFlag(IDevice::PropertyType type, IDevice::PropertyType flag)
{
	return (((int)type) & ((int)flag)) == (int)flag;
}

void DeviceProperty::UpdatePropertyValue(void)
{
	auto type = (IDevice::PropertyType)(((int)pd->Type()) & 0xFF);
	auto enumMode = HasFlag(pd->Type(), IDevice::PropertyType::Enum);
	auto& options = m_LastOptions;
	auto value = pd->GetValue();
	if (options.empty())
	{
		switch (type)
		{
		case IDevice::PropertyType::Int:
		{
			auto iv = std::wcstol(value.data(), nullptr, 10);
			SetValue(iv);
		}
		break;
		case IDevice::PropertyType::Real:
		{
			auto iv = std::wcstod(value.data(), nullptr);
			SetValue(iv);
		}
		break;
		case IDevice::PropertyType::String:
		{
			SetValue(CString(value.data()));
		}
		break;
		case IDevice::PropertyType::Boolean:
		{
			auto iv = std::wcstol(value.data(), nullptr, 10) != 0;
			auto bv = iv ? CString(L"True") : CString(L"False");
			SetValue(bv);
		}
		break;
		}
	}
	else if(enumMode)
	{
		auto displayValue = GetOptionTitle(options, value);
		SetValue(displayValue);
	}
	int subCount = GetSubItemsCount();
	for (int i = 0; i < subCount; ++i)
	{
		auto subProp = dynamic_cast<DeviceProperty*>(GetSubItem(i));
		subProp->UpdatePropertyValue();
	}
}

void DeviceProperty::UpdatePropertyTitle(void)
{
	CString title;
	CString desc;
	splitTitle(LSVT(pd->Title().c_str()), title, desc);
	SetName(title, FALSE);
	SetDescription(desc);

	if (GetOptionCount()>0)
	{
		if (HasFlag(pd->Type(), IDevice::PropertyType::Enum))
		{
			if (m_LastOptions.empty())
				m_LastOptions = pd->Options();
			auto value = pd->GetValue();
			SetValue(GetOptionTitle(m_LastOptions, value));
		}
	}
	int subCount = GetSubItemsCount();
	for (int i = 0; i < subCount; ++i)
	{
		auto subProp = dynamic_cast<DeviceProperty*>(GetSubItem(i));
		subProp->UpdatePropertyTitle();
	}
}

void DeviceProperty::OnDrawValue(CDC* pDC, CRect rect)
{
	Gdiplus::Graphics graphics(*pDC);
	Gdiplus::RectF rcItem((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);
	Gdiplus::StringFormat sf;
	sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
	sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisWord);
	Gdiplus::SolidBrush textBrush(Gdiplus::Color::Black);

	auto fontFamily = CreateUIFontFamily();
	CString value(m_varValue);
	if ((!IsEnabled()) || HasFlag(pd->Type(), IDevice::PropertyType::Group))
	{
		Gdiplus::SolidBrush itemBKBrush(Gdiplus::Color(235, 235, 235));
		graphics.FillRectangle(&itemBKBrush, rcItem);
		Gdiplus::Font fontLabel(fontFamily.get(), 14.0f, Gdiplus::FontStyleItalic, Gdiplus::UnitPixel);
		textBrush.SetColor(Gdiplus::Color(120, 120, 120));
		graphics.DrawString(value, value.GetLength(), &fontLabel, rcItem, &sf, &textBrush);
	}
	else
	{
		Gdiplus::SolidBrush itemBKBrush(Gdiplus::Color::White);
		graphics.FillRectangle(&itemBKBrush, rcItem);
		Gdiplus::Font fontLabel(fontFamily.get(), 14.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		graphics.DrawString(value, value.GetLength(), &fontLabel, rcItem, &sf, &textBrush);
	}
}

void DeviceProperty::OnDrawName(CDC* pDC, CRect rect)
{
	Gdiplus::Graphics graphics(*pDC);
	Gdiplus::RectF rcItem((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);
	Gdiplus::StringFormat sf;
	sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
	sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisWord);
	Gdiplus::SolidBrush textBrush(Gdiplus::Color::Black);

	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font fontLabel(fontFamily.get(), 14.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	if (!IsSelected())
	{
		if ((!IsEnabled()) || HasFlag(pd->Type(), IDevice::PropertyType::Group))
		{
			Gdiplus::SolidBrush itemBKBrush(Gdiplus::Color(235, 235, 235));
			graphics.FillRectangle(&itemBKBrush, rcItem);
			textBrush.SetColor(Gdiplus::Color(120, 120, 120));
			graphics.DrawString(m_strName, m_strName.GetLength(), &fontLabel, rcItem, &sf, &textBrush);
		}
		else
		{
			Gdiplus::SolidBrush itemBKBrush(Gdiplus::Color::White);
			graphics.FillRectangle(&itemBKBrush, rcItem);
			graphics.DrawString(m_strName, m_strName.GetLength(), &fontLabel, rcItem, &sf, &textBrush);
		}
	}
	else
	{
		Gdiplus::SolidBrush itemBKBrush(Gdiplus::Color(0, 122, 204));
		graphics.FillRectangle(&itemBKBrush, rcItem);
		textBrush.SetColor(Gdiplus::Color::White);
		graphics.DrawString(m_strName, m_strName.GetLength(), &fontLabel, rcItem, &sf, &textBrush);
	}
}

void DeviceProperty::OnDrawDescription(CDC* pDC, CRect rect)
{
	Gdiplus::Graphics graphics(*pDC);
	Gdiplus::RectF rcDescription((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
	Gdiplus::StringFormat sf;
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);

	sf.SetAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
	sf.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
	sf.SetTrimming(Gdiplus::StringTrimming::StringTrimmingEllipsisWord);
	Gdiplus::SolidBrush textBrush(Gdiplus::Color::Black);

	auto fontFamily = CreateUIFontFamily();
	Gdiplus::Font font(fontFamily.get(), 14.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	graphics.DrawString(m_strDescr, m_strDescr.GetLength(), &font, rcDescription, &sf, &textBrush);
}

void DeviceProperty::OnClickButton(CPoint point)
{
	if (m_pWndCombo != nullptr)
	{
		m_LastOptions = pd->Options();
		m_pWndCombo->ResetContent();
		for (auto opt : m_LastOptions)
		{
			m_pWndCombo->AddString(LSVT(opt.second.data()));
		}
		SetDroppedAutoWidth();
	}
	CMFCPropertyGridProperty::OnClickButton(point);
}

BOOL DeviceProperty::OnSetCursor() const
{
	return !IsEnabled();
}

void DeviceProperty::OnSelectCombo()
{
	auto item = this->m_pWndCombo->GetCurSel();
	if (item >= 0)
	{
		CString title;
		m_pWndCombo->GetLBText(item, title);
		if (!HasFlag(pd->Type(), IDevice::PropertyType::Enum))
		{
			title = GetOptionValue(m_LastOptions, title);
		}
		SetValue(title);
		m_pWndList->OnPropertyChanged(this);
		return;
	}
}

CString DeviceProperty::GetValueTooltip()
{
	return CString(GetValue());
}

void DeviceProperty::SetDroppedAutoWidth()
{
	if (m_pWndCombo == nullptr)
		return;
	CString str;
	CSize   sz;
	int     dx = 0;
	CDC*    pDC = m_pWndCombo->GetDC();
	auto count = m_pWndCombo->GetCount();
	auto edge = GetSystemMetrics(SM_CXEDGE) * 2 + GetSystemMetrics(SM_CXVSCROLL) * 2;
	for (int i = 0; i < count; i++)
	{
		m_pWndCombo->GetLBText(i, str);
		sz = pDC->GetTextExtent(str);
		if (m_pWndCombo->GetDroppedWidth() < (sz.cx + edge))
		{
			m_pWndCombo->SetDroppedWidth(sz.cx + edge);
		}
	}
	m_pWndCombo->ReleaseDC(pDC);
}


static DeviceProperty* NewProperty(CPropertyTableCtrl::PropertyDescription pd)
{
	auto type = (IDevice::PropertyType)(((int)pd->Type()) & 0xFF);
	auto enumMode = HasFlag(pd->Type(), IDevice::PropertyType::Enum);
	auto isGroup = HasFlag(pd->Type(), IDevice::PropertyType::Group);
	auto options = pd->Options();
	auto defaultVal = pd->DefaultValue();
	CString title;
	CString desc;
	splitTitle(LSVT(pd->Title().c_str()), title, desc);
	DeviceProperty* prop = nullptr;
	if (isGroup)
	{
		prop = new DeviceProperty(title);
		prop->pd = pd;
		for (auto cpd : pd->Childs())
		{
			prop->AddSubItem(NewProperty(cpd));
		}
	}
	else
	{
		auto value = pd->GetValue();
		if (options.empty())
		{
			switch (type)
			{
			case IDevice::PropertyType::Int:
			{
				auto iv = std::wcstol(value.data(), nullptr, 10);
				auto dv = std::wcstol(defaultVal.data(), nullptr, 10);
				prop = new DeviceProperty(title, iv, desc, 0, nullptr, nullptr, L"-1234567890");
				auto smin = pd->MinValue();
				auto smax = pd->MaxValue();
				if ((!smin.empty()) && (!smax.empty()) && (options.empty()))
				{
					auto mi = std::wcstoll(smin.c_str(), nullptr, 10);
					auto mx = std::wcstoll(smax.c_str(), nullptr, 10);
					prop->EnableSpinControl(TRUE, (int)mi, (int)mx);
				}
			}
			break;
			case IDevice::PropertyType::Real:
			{
				auto iv = std::wcstod(value.data(), nullptr);
				auto dv = std::wcstod(defaultVal.data(), nullptr);
				prop = new DeviceProperty(title, iv, desc, 0, nullptr, nullptr, L"-1234567890.");
			}
			break;
			case IDevice::PropertyType::String:
			{
				prop = new DeviceProperty(title, CString(value.data()), desc);
			}
			break;
			}
		}
		else
		{
			std::wstring displayValue;
			if (enumMode)
				displayValue = GetOptionTitle(options, value);
			else
				displayValue = value;

			prop = new DeviceProperty(title, CString(displayValue.data()), desc);
			prop->RemoveAllOptions();
			if (enumMode)
				prop->AllowEdit(FALSE);
			prop->AddOption(L" ");
		}

		if (prop != nullptr)
		{
			if (pd->ChangeFlags() == IDevice::PropertyChangeFlags::Readonly)
			{
				prop->Enable(FALSE);
			}
			prop->pd = pd;
		}
	}
	return prop;
}

static void UpdatePropertyStatus(DeviceProperty* prop, bool started)
{
	auto subCount = prop->GetSubItemsCount();
	for (int i = 0; i < subCount; ++i)
	{
		auto subprop = dynamic_cast<DeviceProperty*>(prop->GetSubItem(i));
		UpdatePropertyStatus(subprop, started);
	}

	auto pd = prop->pd;
	auto flags = pd->ChangeFlags();
	if (flags == IDevice::PropertyChangeFlags::CanChangeAlways
		|| flags == IDevice::PropertyChangeFlags::Readonly)
		return;

	if (started)
	{
		if (flags == IDevice::PropertyChangeFlags::CanChangeAfterStart)
			prop->Enable(TRUE);
		else if (flags == IDevice::PropertyChangeFlags::CanChangeBeforeStart)
			prop->Enable(FALSE);
	}
	else
	{
		if (flags == IDevice::PropertyChangeFlags::CanChangeAfterStart)
			prop->Enable(FALSE);
		else if (flags == IDevice::PropertyChangeFlags::CanChangeBeforeStart)
			prop->Enable(TRUE);
	}
}
void CPropertyTableCtrl::UpdateDeviceStatus(bool started)
{
	for (int i = 0; i < GetPropertyCount(); ++i)
	{
		auto prop = dynamic_cast<DeviceProperty*>(GetProperty(i));
		UpdatePropertyStatus(prop, started);
	}
}

void CPropertyTableCtrl::Clear()
{
	this->RemoveAll();
}

void CPropertyTableCtrl::UpdateProperties()
{
	DeviceProperty* prop = nullptr;
	for (int i = 0; i < GetPropertyCount(); ++i)
	{
		auto prop = dynamic_cast<DeviceProperty*>(GetProperty(i));
		prop->UpdatePropertyValue();
	}
}

void CPropertyTableCtrl::UpdatePropertiesTitle()
{
	DeviceProperty* prop = nullptr;
	for (int i = 0; i < GetPropertyCount(); ++i)
	{
		auto prop = dynamic_cast<DeviceProperty*>(GetProperty(i));
		prop->UpdatePropertyTitle();
	}
}

void CPropertyTableCtrl::AddProperty(PropertyDescription pd)
{
	auto prop = NewProperty(pd);
	CMFCPropertyGridCtrl::AddProperty(prop);
}

void CPropertyTableCtrl::OnPropertyChanged(CMFCPropertyGridProperty* pProp) const
{
	auto prop = dynamic_cast<DeviceProperty*>(pProp);
	auto pd = prop->pd;
	CString title(prop->GetValue());
	auto enumMode = HasFlag(pd->Type(), IDevice::PropertyType::Enum);
	std::wstring value;
	if (enumMode)
	{
		value = GetOptionValue(prop->m_LastOptions, title);
	}
	else
	{
		value = title.GetString();
	}
	::SendMessage(::GetParent(GetSafeHwnd()), kWM_CDEVICE_PROPERTY_CHANGED, (WPARAM)(prop), (LPARAM)(value.data()));
}

void CPropertyTableCtrl::OnDrawBorder(CDC* pDC)
{
	Gdiplus::Graphics graphics(*pDC);
	CRect rect;
	GetClientRect(&rect);
	Gdiplus::RectF rcItem((Gdiplus::REAL)rect.left, (Gdiplus::REAL)rect.top, (Gdiplus::REAL)rect.Width(), (Gdiplus::REAL)rect.Height());
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintClearTypeGridFit);

	Gdiplus::Pen pen(Gdiplus::Color(60, 0, 0, 0), 1);
	
	rcItem.Width -= 1;
	rcItem.Height -= 1;
	graphics.DrawRectangle(&pen, rcItem);
}
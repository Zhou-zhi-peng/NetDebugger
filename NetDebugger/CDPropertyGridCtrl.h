#pragma once
#include "IAsyncStream.h"

class CPropertyTableCtrl : public CMFCPropertyGridCtrl
{
public:
	using PropertyDescription = IDevice::PD;
	void UpdateProperties();
	void UpdatePropertiesTitle();
	void Clear();
	void AddProperty(PropertyDescription pd);
	void UpdateDeviceStatus(bool started);
protected:
	virtual void OnPropertyChanged(CMFCPropertyGridProperty* pProp) const override;
	virtual void OnDrawBorder(CDC* pDC) override;
};

class DeviceProperty : public CMFCPropertyGridProperty
{
	friend class CPropertyTableCtrl;
public:
	using CMFCPropertyGridProperty::CMFCPropertyGridProperty;
public:
	CPropertyTableCtrl::PropertyDescription pd;
public:
	void UpdatePropertyValue(void);
	void UpdatePropertyTitle(void);
protected:
	virtual void OnDrawValue(CDC* pDC, CRect rect) override;

	virtual void OnDrawName(CDC* pDC, CRect rect) override;

	virtual void OnDrawDescription(CDC* pDC, CRect rect) override;

	virtual void OnClickButton(CPoint point) override;

	virtual BOOL OnSetCursor() const override;

	virtual void OnSelectCombo() override;

	virtual CString GetValueTooltip() override;
private:
	void SetDroppedAutoWidth();
private:
	IDevice::PropertyDescription::EnumOptions m_LastOptions;
};
#pragma once
#include <afxwin.h>
class CRealTimeStatusCtrl :
	public CWnd
{
public:
	CRealTimeStatusCtrl();
	virtual ~CRealTimeStatusCtrl();
public:
	void Clear(void)
	{
		m_ReadSpeeds.clear();
		m_WriteSpeeds.clear();
		m_MaxReadSpeed = 0;
		m_MaxWriteSpeed = 0;
		m_readBytes = 0;
		m_writeBytes = 0;
		m_lastUpdateTime = std::chrono::high_resolution_clock::now();
	}
	void UpdateStatistics(bool connected, uint64_t readBytes, uint64_t writeBytes);
private:
	void AddSpeedPoint(uint64_t readspeed, uint64_t writespeed);
	int DrawStatisicsString(Gdiplus::Graphics& graphics, const Gdiplus::RectF& clientRect);
	void DrawSpeedCurve(Gdiplus::Graphics& graphics, const Gdiplus::RectF& clientRect);
	void DrawDisconnectedStatus(Gdiplus::Graphics& graphics, const Gdiplus::RectF& clientRect);
private:
	CString GetReadSpeedString(void) const;
	CString GetWriteSpeedString(void) const;
private:
	bool m_connected;
	uint64_t m_readBytes;
	uint64_t m_writeBytes;
	uint64_t m_MaxReadSpeed;
	uint64_t m_MaxWriteSpeed;
	uint64_t m_AveReadSpeed;
	uint64_t m_AveWriteSpeed;
	std::vector<uint64_t> m_ReadSpeeds;
	std::vector<uint64_t> m_WriteSpeeds;
	std::chrono::high_resolution_clock::time_point m_lastUpdateTime;
public:
	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
};


#pragma once

#include "IDeviceUI.h"
#include "CSelectControl.h"
#include "CEditEx.h"
// CTextSendEditor

class CTextSendEditor :  public ISendEditor
{
public:
	DECLARE_DYNAMIC(CTextSendEditor)
	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TEXT_SEND_EDITOR };
#endif

public:
	CTextSendEditor(CWnd* pParentWnd);
	virtual ~CTextSendEditor();
public:
	// 通过 ISendEditor 继承
	virtual UINT GetDataType(void) override;
	virtual void SetDataType(UINT type) override;
	virtual void GetDataBuffer(std::function<bool(const uint8_t* buffer, size_t size)> handler) override;
	virtual void ClearContent(void) override;
	virtual void SetContent(const void * data, size_t size) override;
	virtual void Create(void) override;
	virtual void Destroy(void) override;
	virtual void UpdateLanguage(void) override;
protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV 支持
	virtual BOOL OnInitDialog(void) override;
private:
	CWnd* m_ParentWnd;
	CSelectControl m_SendDisplayTypeCtrl;
	CRichEditEx m_SendEditCtrl;
	DataBuffer m_DataBuffer;
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnEditDisplayTypeChanged(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedButtonSendClear();
};



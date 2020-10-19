#pragma once

#ifdef _WIN32_WCE
#error "Windows CE 不支持 CDHtmlDialog。"
#endif

// CHelpDialog 对话框

class CHelpDialog : public CDHtmlDialog
{
	DECLARE_DYNCREATE(CHelpDialog)

public:
	CHelpDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CHelpDialog();
// 重写
//	HRESULT OnButtonOK(IHTMLElement *pElement);
//	HRESULT OnButtonCancel(IHTMLElement *pElement);

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_HELP_DIALOG, IDH = IDR_HTML_HELP};
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV 支持
	virtual BOOL OnInitDialog() override;
	virtual BOOL CanAccessExternal() override;
	DECLARE_MESSAGE_MAP()
	DECLARE_DHTML_EVENT_MAP()
};

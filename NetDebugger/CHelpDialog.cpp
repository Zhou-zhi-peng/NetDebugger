// CHelpDialog.cpp: 实现文件
//

#include "pch.h"
#include "NetDebugger.h"
#include "CHelpDialog.h"


// CHelpDialog 对话框

IMPLEMENT_DYNCREATE(CHelpDialog, CDHtmlDialog)

CHelpDialog::CHelpDialog(CWnd* pParent /*=nullptr*/)
	: CDHtmlDialog(IDD_HELP_DIALOG, IDR_HTML_HELP, pParent)
{

}

CHelpDialog::~CHelpDialog()
{
}

void CHelpDialog::DoDataExchange(CDataExchange* pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

BOOL CHelpDialog::OnInitDialog()
{
	CDHtmlDialog::OnInitDialog();
	//SetExternalDispatch(this);
	return TRUE;
}

BOOL CHelpDialog::CanAccessExternal()
{
	return TRUE;
}

BEGIN_MESSAGE_MAP(CHelpDialog, CDHtmlDialog)
END_MESSAGE_MAP()

BEGIN_DHTML_EVENT_MAP(CHelpDialog)
END_DHTML_EVENT_MAP()

//DHTML_EVENT_ONCLICK(_T("ButtonOK"), OnButtonOK)
//DHTML_EVENT_ONCLICK(_T("ButtonCancel"), OnButtonCancel)

// CHelpDialog 消息处理程序

//HRESULT CHelpDialog::OnButtonOK(IHTMLElement* /*pElement*/)
//{
//	OnOK();
//	return S_OK;
//}
//
//HRESULT CHelpDialog::OnButtonCancel(IHTMLElement* /*pElement*/)
//{
//	OnCancel();
//	return S_OK;
//}

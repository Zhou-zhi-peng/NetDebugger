// CSettingDlg.cpp: 实现文件
//

#include "pch.h"
#include "NetDebugger.h"
#include "CSettingDlg.h"
#include "afxdialogex.h"


// CSettingDlg 对话框

IMPLEMENT_DYNAMIC(CSettingDlg, CDialogEx)

CSettingDlg::CSettingDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SETTING_DIALOG, pParent)
{

}

CSettingDlg::~CSettingDlg()
{
}

void CSettingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_LANGUAGE, m_LanguageCtrl);
}


BEGIN_MESSAGE_MAP(CSettingDlg, CDialogEx)
END_MESSAGE_MAP()


BOOL CSettingDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_Languages = theApp.GetLS().Names();
	auto curLid = theApp.GetLS().GetLanguage();
	int index = 0;
	//auto wndLC = ::GetDlgItem(m_hWnd, IDC_COMBO_LANGUAGE);
	for (auto n : m_Languages)
	{
		auto item = m_LanguageCtrl.AddString(n.second);
		m_LanguageCtrl.SetItemData(item, index);
		if (n.first == curLid)
			m_LanguageCtrl.SetCurSel(item);
		++index;
	}
	return TRUE;
}

void CSettingDlg::OnOK()
{
	UpdateData(TRUE);
	auto sel = m_LanguageCtrl.GetCurSel();
	if (sel >= 0)
	{
		auto i = (int)m_LanguageCtrl.GetItemData(sel);
		theApp.GetLS().SetLanguage(m_Languages[i].first);
	}
	CDialogEx::OnOK();
}

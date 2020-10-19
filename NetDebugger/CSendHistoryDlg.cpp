// CSendHistoryDlg.cpp: 实现文件
//

#include "pch.h"
#include "NetDebugger.h"
#include "CSendHistoryDlg.h"
#include "afxdialogex.h"
#include "TextEncodeType.h"
// CSendHistoryDlg 对话框

IMPLEMENT_DYNAMIC(CSendHistoryDlg, CDialogEx)

CSendHistoryDlg::CSendHistoryDlg(std::vector<std::shared_ptr<SendHistoryRecord>>& historys, CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SEND_HISTORY_DIALOG, pParent),
	m_HistoryRecords(historys)
{

}

CSendHistoryDlg::~CSendHistoryDlg()
{
}

void CSendHistoryDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DATA_DISPLAY_MODE, m_DataDisplayModeCtrl);
	DDX_Control(pDX, IDC_DATA_EDIT_BOX, m_DataEditCtrl);
	DDX_Control(pDX, IDC_LIST_HISTORY, m_HistoryListCtrl);
}


BEGIN_MESSAGE_MAP(CSendHistoryDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR, &CSendHistoryDlg::OnBnClickedButtonClear)
	ON_BN_CLICKED(IDOK, &CSendHistoryDlg::OnBnClickedOk)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_HISTORY, &CSendHistoryDlg::OnLvnItemchangedListHistory)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_HISTORY, &CSendHistoryDlg::OnNMDblclkListHistory)
END_MESSAGE_MAP()


// CSendHistoryDlg 消息处理程序


static CString ToStringName(std::shared_ptr<SendHistoryRecord> history)
{
	CString result;
	auto p = history->DataBuffer.data();
	auto l = history->DataBuffer.size() > 32 ? 32 : history->DataBuffer.size();
	std::vector<uint8_t> temp(p, p + l);
	auto data = Transform::DecodeToWString(temp, (TextEncodeType)(history->TextEncoding));
	result.SetString(data.data(), (int)data.length());
	return result;
}

BOOL CSendHistoryDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	for (int i = 0; i < (int)TextEncodeType::_MAX; ++i)
	{
		m_DataDisplayModeCtrl.AddButton(TextEncodeTypeName((TextEncodeType)i), i);
	}
	m_DataDisplayModeCtrl.SetValue((UINT)TextEncodeType::ASCII);
	m_DataDisplayModeCtrl.EnableWindow(FALSE);

	m_HistoryListCtrl.InsertColumn(0, LSTEXT(SEND.HISTORY.DIALOG.TYPE), 0, 100);
	m_HistoryListCtrl.InsertColumn(1, LSTEXT(SEND.HISTORY.DIALOG.DATA), 0, 260);

	m_HistoryListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	int iItem = 0;
	for (auto history : m_HistoryRecords)
	{
		m_HistoryListCtrl.InsertItem(iItem,TextEncodeTypeName((TextEncodeType)(history->TextEncoding)));
		m_HistoryListCtrl.SetItemText(iItem, 1, ToStringName(history));
		m_HistoryListCtrl.SetItemData(iItem, (DWORD_PTR)history.get());
		++iItem;
	}
	m_HistoryListCtrl.SetHotItem(0);

	m_DataEditCtrl.SetReadOnly(TRUE);

	SetWindowText(LSTEXT(SEND.HISTORY.DIALOG.TITLE));
	SetDlgItemText(IDOK, LSTEXT(SEND.HISTORY.DIALOG.OK));
	SetDlgItemText(IDC_BUTTON_CLEAR, LSTEXT(SEND.HISTORY.DIALOG.CLEAR));
	return TRUE;
}


void CSendHistoryDlg::OnBnClickedButtonClear()
{
	m_HistoryListCtrl.DeleteAllItems();
	m_HistoryRecords.clear();
	m_Result = nullptr;
	CDialogEx::OnCancel();
}


void CSendHistoryDlg::OnBnClickedOk()
{
	auto i = m_HistoryListCtrl.GetSelectionMark();
	if (i >= 0 && i< (int)m_HistoryRecords.size())
	{
		m_Result = m_HistoryRecords[i];
	}
	else
	{
		m_Result = nullptr;
	}
	CDialogEx::OnOK();
}



void CSendHistoryDlg::OnLvnItemchangedListHistory(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	auto i = pNMLV->iItem;
	if (i >= 0 && i < (int)m_HistoryRecords.size())
	{
		m_Result = m_HistoryRecords[i];
		m_DataDisplayModeCtrl.SetValue((UINT)m_Result->TextEncoding);
		m_DataEditCtrl.SetReadOnly(FALSE);
		m_DataEditCtrl.SetText(Transform::DecodeToWString(m_Result->DataBuffer, (TextEncodeType)m_Result->TextEncoding));
		m_DataEditCtrl.SetReadOnly(TRUE);
	}
	*pResult = 0;
}

void CSendHistoryDlg::OnNMDblclkListHistory(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	OnBnClickedOk();
	*pResult = 0;
}

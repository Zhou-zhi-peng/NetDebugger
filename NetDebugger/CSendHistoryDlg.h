#pragma once
#include "CSelectControl.h"
#include "CEditEx.h"
// CSendHistoryDlg 对话框

class SendHistoryRecord
{
public:
	int TextEncoding;
	std::vector<uint8_t> DataBuffer;

	SendHistoryRecord() :
		TextEncoding(0)
	{

	}
	~SendHistoryRecord() = default;

	bool operator == (const SendHistoryRecord& b) const
	{
		return TextEncoding == b.TextEncoding
			&& DataBuffer.size() == b.DataBuffer.size()
			&& (memcmp(DataBuffer.data(), b.DataBuffer.data(), b.DataBuffer.size()) == 0);
	}

	bool operator != (const SendHistoryRecord& b) const
	{
		return TextEncoding != b.TextEncoding
			|| DataBuffer.size() != b.DataBuffer.size()
			|| (memcmp(DataBuffer.data(), b.DataBuffer.data(), b.DataBuffer.size()) != 0);
	}
};

class CSendHistoryDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CSendHistoryDlg)

public:
	CSendHistoryDlg(std::vector<std::shared_ptr<SendHistoryRecord>>& historys, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CSendHistoryDlg();
public:
	std::shared_ptr<SendHistoryRecord> GetResult(void)const { return m_Result; }
// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SEND_HISTORY_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
private:
	CSelectControl m_DataDisplayModeCtrl;
	CRichEditEx m_DataEditCtrl;
	CListCtrl m_HistoryListCtrl;
	std::shared_ptr<SendHistoryRecord> m_Result;
	std::vector<std::shared_ptr<SendHistoryRecord>>& m_HistoryRecords;
public:
	afx_msg void OnBnClickedButtonClear();
	afx_msg void OnBnClickedOk();
	afx_msg void OnLvnItemchangedListHistory(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMDblclkListHistory(NMHDR *pNMHDR, LRESULT *pResult);
};

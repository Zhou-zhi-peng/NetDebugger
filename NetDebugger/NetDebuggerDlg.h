
// NetDebuggerDlg.h: 头文件
//

#pragma once
#include "CSelectControl.h"
#include "CDPropertyGridCtrl.h"
#include "CRealTimeStatusCtrl.h"
#include "CPlaceholderEdit.h"
#include "CEditEx.h"
#include "IDeviceUI.h"
#include "IndicatorButton.h"
#include "IAsyncStream.h"
#include "BlockingQueue.hpp"
#include "DataBuffer.h"

class FileSendContext;
class SendHistoryRecord;
// CNetDebuggerDlg 对话框
class CNetDebuggerDlg : public CDialogEx
{
// 构造
public:
	CNetDebuggerDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NETDEBUGGER_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持
private:
	void SetControlText(UINT id, const std::wstring& text);
	void SetControlText(UINT id, const CString& text);
	void SetControlEnable(UINT id, bool enable);
private:
	void InitDisplayTypeCtrl(CSelectControl& ctrl);
	void OnDeviceStatusChanged(IDevice::DeviceStatus status,const std::wstring& message);
	void OnDevicePropertyChanged(void);
	void OnDeviceChannelConnected(std::shared_ptr<IAsyncChannel> channel, const std::wstring& message);
	void OnDeviceChannelDisconnected(std::shared_ptr<IAsyncChannel> channel, const std::wstring& message);
private:
	void ReadChannelData(std::shared_ptr<IAsyncChannel> channel, std::shared_ptr<std::vector<uint8_t>> buffer);
	void SendDataToChannel(std::shared_ptr<IAsyncChannel> channel, const void* buffer, size_t size, IAsyncChannel::IoCompletionHandler cphandler);
	void SendFileBlockToChannel(std::shared_ptr<FileSendContext> ctx);
	void StartSendFileToChannel(std::shared_ptr<IAsyncChannel> channel, const CString& filename);
private:
	void LoadSendHistory(void);
	void SaveSendHistory(void);
protected:
	void SendUIThreadTask(std::function<void()> task);
	void PostUIThreadTask(std::function<void()> task);
	void AppendDataToRecvBuffer(std::shared_ptr<IAsyncChannel> channel, std::shared_ptr<std::vector<uint8_t>> buffer);
	void AppendSendHistory(UINT type, std::shared_ptr<std::vector<uint8_t>> buffer);
	void SaveReadHistory(const CString& path,bool tip, bool append, bool lockBuffer);

	void UpdateUILangText(void);
// 实现
private:
	struct ReceivedMessage
	{
		std::string label;
		std::string message;
	};
	
	HICON m_hIcon;
	CSize m_MinSize;
	CComboBoxEx m_DeviceTypeCtrl;
	CComboBoxEx m_ChannelsCtrl;
	CSelectControl m_RecvDisplayTypeCtrl;
	CRichEditEx m_RecvEditCtrl;
	CPropertyTableCtrl m_DevicePropertyPanel;
	CRealTimeStatusCtrl m_DeviceStatisticsCtrl;
	CComboBox m_MemoryMaxCtrl;
	CButton m_ShowRecvDataCtrl;
	CButton m_AutoSaveCtrl;
	CMFCEditBrowseCtrl m_AutoSaveFilePathCtrl;
	CIndicatorButton m_ConnectButton;
	CButton m_RecvInfoAdditionalCtrl;
	CPlaceholderEdit m_AutoSendIntervalCtrl;
	CMFCToolTipCtrl m_ToolTipCtrl;
	std::unique_ptr<ISendEditor> m_SendEditor;
	std::shared_ptr<IDevice> m_CDevice;
	std::map<std::wstring, std::shared_ptr<IAsyncChannel>> m_ChannelsMap;
	int m_ConnectStatusPop;
	size_t m_MaxReadMemorySize;
	std::atomic<uint64_t> m_ReadByteCount;
	std::atomic<uint64_t> m_WriteByteCount;
	bool m_bAutoSave;
	bool m_bRecvInfoAdditional;
	bool m_bShowRecvData;
	DataBuffer m_ReadBuffer;
	std::mutex m_ReadBufferMutex;
	std::vector<std::shared_ptr<SendHistoryRecord>> m_HistoryRecords;
	BlockingQueue<ReceivedMessage*> m_ReceivedMessageQueue;
	std::atomic<bool> m_Closed;
	std::vector<std::function<void()>> m_UILUpdates;
protected:
	// 生成的消息映射函数
	DECLARE_MESSAGE_MAP()

	virtual void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnCbnSelchangeCmbNetType();
	afx_msg LRESULT OnDevicePropertyChanged(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnEditDisplayTypeChanged(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnUIThreadTask(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedButtonClearStatistics();
	afx_msg void OnBnClickedButtonRecvClear();
	afx_msg void OnBnClickedButtonSendFile();
	afx_msg void OnBnClickedButtonRecvSave();
	afx_msg void OnBnClickedButtonSendHistory();
	afx_msg void OnCbnSelchangeComboMemoryMax();
	afx_msg void OnBnClickedCheckAutoSave();
	afx_msg void OnBnClickedButtonCloseChannel();
	afx_msg void OnBnClickedCheckAutoAdditional();
	afx_msg void OnEnChangeEditSendInterval();
	afx_msg void OnBnClickedCheckShowRecvdata();
};

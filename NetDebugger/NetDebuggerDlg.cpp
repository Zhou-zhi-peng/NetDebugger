
// NetDebuggerDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "afxdialogex.h"
#include "NetDebugger.h"
#include "NetDebuggerDlg.h"
#include "CSettingDlg.h"
#include "CTextSendEditor.h"
#include "IAsyncStream.h"
#include "PopWindow.h"
#include "UserWMDefine.h"
#include "CSendHistoryDlg.h"
#include "TextEncodeType.h"
#include "GdiplusAux.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
constexpr UINT kSTATISTICS_TIMER_ID = 0;
constexpr UINT kAUTO_SEND_TIMER_ID = 1;
constexpr UINT kSTATISTICS_UPDATE_TIME = 1000;
constexpr size_t kFILE_IO_BLOCK_SIZE = 1024 * 8;


class FileSendContext
{
public:
	FileSendContext() :
		channel(nullptr),
		wid(-1),
		cancel(false),
		stoped(true),
		dataSize(0),
		fileLength(0),
		sentLength(0)
	{
		file.m_hFile = INVALID_HANDLE_VALUE;
		nextUpdateUI = std::chrono::system_clock::now();
	}
	~FileSendContext()
	{
		if(file.m_hFile != INVALID_HANDLE_VALUE)
			file.Close();
	}
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
	std::shared_ptr<IAsyncChannel> channel;
	int wid;
	std::atomic<bool> cancel;
	std::atomic<bool> stoped;
	TimePoint nextUpdateUI;
	CFile file;
	ULONGLONG fileLength;
	ULONGLONG sentLength;
	size_t dataSize;
	uint8_t buffer[kFILE_IO_BLOCK_SIZE];
};

template <class T, class K>
static std::vector<T> ToVector(std::map<K, T> map)
{
	std::vector<T> channels;
	for (auto kv : map)
	{
		channels.push_back(kv.second);
	}
	return channels;
}

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL OnInitDialog(void);

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BOOL CAboutDlg::OnInitDialog(void)
{
	SetWindowText(LSTEXT(ABOUT.DIALOG.TITLE));
	SetDlgItemText(IDC_STATIC_V, LSTEXT(ABOUT.DIALOG.STATIC.VERSION));
	SetDlgItemText(IDC_STATIC_C, LSTEXT(ABOUT.DIALOG.STATIC.COPYRIGHT));
	return TRUE;
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CNetDebuggerDlg 对话框



CNetDebuggerDlg::CNetDebuggerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_NETDEBUGGER_DIALOG, pParent),
	m_ConnectStatusPop(-1),
	m_MaxReadMemorySize(0),
	m_ReadByteCount(0),
	m_WriteByteCount(0),
	m_bShowRecvData(true),
	m_bAutoSave(false),
	m_bRecvInfoAdditional(false),
	m_ReadBuffer(),
	m_ReadBufferMutex(),
	m_HistoryRecords(),
	m_ReceivedMessageQueue(4096),
	m_Closed(false),
	m_UILUpdates(),
	m_SendEditor(nullptr)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CNetDebuggerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CMB_NET_TYPE, m_DeviceTypeCtrl);
	DDX_Control(pDX, IDC_RECV_DISPLAY_TYPE, m_RecvDisplayTypeCtrl);
	DDX_Control(pDX, IDC_RECV_EDITBOX, m_RecvEditCtrl);
	DDX_Control(pDX, IDC_NET_DEVICE_SETTING, m_DevicePropertyPanel);
	DDX_Control(pDX, IDC_DEVICE_STATISTICS, m_DeviceStatisticsCtrl);
	DDX_Control(pDX, IDC_CMB_CHANNELS, m_ChannelsCtrl);
	DDX_Control(pDX, IDC_COMBO_MEMORY_MAX, m_MemoryMaxCtrl);
	DDX_Control(pDX, IDC_CHECK_AUTO_SAVE, m_AutoSaveCtrl);
	DDX_Control(pDX, IDC_FILE_PATH, m_AutoSaveFilePathCtrl);
	DDX_Control(pDX, IDC_BUTTON_CONNECT, m_ConnectButton);
	DDX_Control(pDX, IDC_CHECK_AUTO_ADDITIONAL, m_RecvInfoAdditionalCtrl);
	DDX_Control(pDX, IDC_EDIT_SEND_INTERVAL, m_AutoSendIntervalCtrl);
	DDX_Control(pDX, IDC_CHECK_SHOW_RECVDATA, m_ShowRecvDataCtrl);
}

BEGIN_MESSAGE_MAP(CNetDebuggerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_GETMINMAXINFO()
	ON_CBN_SELCHANGE(IDC_CMB_NET_TYPE, &CNetDebuggerDlg::OnCbnSelchangeCmbNetType)
	ON_MESSAGE(kWM_CDEVICE_PROPERTY_CHANGED, &CNetDebuggerDlg::OnDevicePropertyChanged)
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CNetDebuggerDlg::OnBnClickedButtonConnect)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_MESSAGE(kWM_UI_THREAD_TASK, &CNetDebuggerDlg::OnUIThreadTask)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CNetDebuggerDlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR_STATISTICS, &CNetDebuggerDlg::OnBnClickedButtonClearStatistics)
	ON_BN_CLICKED(IDC_BUTTON_RECV_CLEAR, &CNetDebuggerDlg::OnBnClickedButtonRecvClear)
	ON_BN_CLICKED(IDC_BUTTON_SEND_FILE, &CNetDebuggerDlg::OnBnClickedButtonSendFile)
	ON_BN_CLICKED(IDC_BUTTON_RECV_SAVE, &CNetDebuggerDlg::OnBnClickedButtonRecvSave)
	ON_BN_CLICKED(IDC_BUTTON_SEND_HISTORY, &CNetDebuggerDlg::OnBnClickedButtonSendHistory)
	ON_CBN_SELCHANGE(IDC_COMBO_MEMORY_MAX, &CNetDebuggerDlg::OnCbnSelchangeComboMemoryMax)
	ON_BN_CLICKED(IDC_CHECK_AUTO_SAVE, &CNetDebuggerDlg::OnBnClickedCheckAutoSave)
	ON_BN_CLICKED(IDC_BUTTON_CLOSE_CHANNEL, &CNetDebuggerDlg::OnBnClickedButtonCloseChannel)
	ON_BN_CLICKED(IDC_CHECK_AUTO_ADDITIONAL, &CNetDebuggerDlg::OnBnClickedCheckAutoAdditional)
	ON_EN_CHANGE(IDC_EDIT_SEND_INTERVAL, &CNetDebuggerDlg::OnEnChangeEditSendInterval)
	ON_BN_CLICKED(IDC_CHECK_SHOW_RECVDATA, &CNetDebuggerDlg::OnBnClickedCheckShowRecvdata)
END_MESSAGE_MAP()


static int AddComboBoxString(CComboBoxEx& ctrl, const CString& text, void* pData = nullptr)
{
	COMBOBOXEXITEM item;
	item.mask = CBEIF_TEXT;
	item.iItem = ctrl.GetCount();
	item.pszText = const_cast<LPTSTR>(text.GetString());
	item.cchTextMax = text.GetLength();
	item.iImage = 0;
	item.iSelectedImage = 0;
	item.iOverlay = 0;
	item.iIndent = 0;
	auto idx = ctrl.InsertItem(&item);
	ctrl.SetItemDataPtr(idx, pData);
	return idx;
}

static void RemoveComboBoxString(CComboBoxEx& ctrl, const CString& text)
{
	auto x = ctrl.FindString(0, text);
	if (x >= 0)
	{
		ctrl.DeleteString(x);
	}
}

static void UpdateComboBoxString(CComboBoxEx& ctrl, int index, const CString& text)
{
	COMBOBOXEXITEM item;
	item.mask = CBEIF_TEXT;
	item.iItem = index;
	item.pszText = const_cast<LPTSTR>(text.GetString());
	item.cchTextMax = text.GetLength();
	item.iImage = 0;
	item.iSelectedImage = 0;
	item.iOverlay = 0;
	item.iIndent = 0;
	auto data = ctrl.GetItemDataPtr(index);
	ctrl.SetItem(&item);
	ctrl.SetItemDataPtr(index, data);
}

void CNetDebuggerDlg::LoadSendHistory(void)
{
	constexpr int kMAX_MEMBLOCK_SIZE = 1024 * 1024 * 32;
	auto iHistoryCount = theApp.GetProfileInt(L"SendHistorys", L"HistoryCount", 0);
	UINT iHistorySize = -1;
	LPBYTE pData = nullptr;
	theApp.GetProfileBinary(L"SendHistorys", L"HistoryData", &pData, &iHistorySize);
	if (iHistorySize > kMAX_MEMBLOCK_SIZE)
		return;
	auto ptr = pData;
	auto end = ptr + iHistorySize;
	for (int i = 0; i < (int)iHistoryCount; ++i)
	{
		auto history = std::make_shared<SendHistoryRecord>();
		history->TextEncoding = *reinterpret_cast<int*>(ptr); ptr += sizeof(int); if (ptr >= end) break;
		auto datasize = *reinterpret_cast<int*>(ptr); 
		ptr += sizeof(int); 
		if (ptr >= end) 
			break;
		if (datasize > kMAX_MEMBLOCK_SIZE)
			break;
		history->DataBuffer.resize(datasize);
		memcpy(history->DataBuffer.data(), ptr, datasize); 
		m_HistoryRecords.push_back(history);
		ptr += datasize;
		if (ptr >= end)
			break;
	}
	delete[] pData;
}

void CNetDebuggerDlg::SaveSendHistory(void)
{
	std::vector<uint8_t> buffer;
	for (auto history : m_HistoryRecords)
	{
		auto idx = buffer.size();
		buffer.resize(buffer.size() + sizeof(int));
		auto ptr = buffer.data() + idx;
		*reinterpret_cast<int*>(ptr) = history->TextEncoding;
		idx = buffer.size();
		buffer.resize(buffer.size() + sizeof(int));
		ptr = buffer.data() + idx;
		*reinterpret_cast<int*>(ptr) = (int)history->DataBuffer.size();
		
		idx = buffer.size();
		buffer.resize(buffer.size() + history->DataBuffer.size());
		ptr = buffer.data() + idx;
		memcpy(ptr, history->DataBuffer.data(), history->DataBuffer.size());
	}
	theApp.WriteProfileBinary(L"SendHistorys", L"HistoryData", buffer.data(), (int)buffer.size());
	theApp.WriteProfileInt(L"SendHistorys", L"HistoryCount", (int)m_HistoryRecords.size());
}

void CNetDebuggerDlg::UpdateUILangText(void)
{
	for (auto h : m_UILUpdates)
	{
		h();
	}
	this->RedrawWindow();
}

// CNetDebuggerDlg 消息处理程序
BOOL CNetDebuggerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		pSysMenu->AppendMenu(MF_SEPARATOR);
		pSysMenu->AppendMenu(MF_STRING, IDM_SETTING, LSTEXT(MENU.POP.SETTING));
		pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, LSTEXT(MENU.POP.ABOUTAPP));

		m_UILUpdates.push_back([this]()
		{
			CMenu* pSysMenu = GetSystemMenu(FALSE);
			pSysMenu->ModifyMenu(IDM_SETTING, MF_BYCOMMAND, IDM_SETTING, LSTEXT(MENU.POP.SETTING));
			pSysMenu->ModifyMenu(IDM_ABOUTBOX, MF_BYCOMMAND, IDM_ABOUTBOX, LSTEXT(MENU.POP.ABOUTAPP));
		});
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);
	{
		auto container = GetDlgItem(IDC_SEND_EDIROR);
		auto editor = new CTextSendEditor(container);
		m_SendEditor.reset(editor);
		editor->Create();
		editor->ShowWindow(SW_SHOW);
		editor->EnableWindow();
	}
	m_ToolTipCtrl.Create(this);

	CMFCToolTipInfo params;
	params.m_bVislManagerTheme = FALSE;
	params.m_bBoldLabel = FALSE;
	params.m_bDrawDescription = FALSE;
	params.m_bDrawIcon = FALSE;
	params.m_bRoundedCorners = TRUE;
	params.m_bDrawSeparator = FALSE;
	params.m_clrFill = RGB(255, 255, 255);
	params.m_clrFillGradient = RGB(228, 228, 240);
	params.m_clrText = RGB(61, 83, 80);
	params.m_clrBorder = RGB(144, 149, 168);
	m_ToolTipCtrl.SetParams(&params);
	m_ToolTipCtrl.SetFixedWidth(280, 280);

	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_CONNECT), LSTEXT(TOOLTIP.BTN.CONNECT));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_CLEAR_STATISTICS), LSTEXT(TOOLTIP.BTN.CLEARSTAT));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_COMBO_MEMORY_MAX), LSTEXT(TOOLTIP.COMBO.MEMORY.MAX));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_CHECK_SHOW_RECVDATA), LSTEXT(TOOLTIP.CHECK.SHOW.RECVDATA));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_CHECK_AUTO_ADDITIONAL), LSTEXT(TOOLTIP.CHECK.AUTO.ADDITIONAL));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_CHECK_AUTO_SAVE), LSTEXT(TOOLTIP.CHECK.AUTO.SAVE));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_FILE_PATH), LSTEXT(TOOLTIP.FILE.PATH));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_RECV_DISPLAY_TYPE), LSTEXT(TOOLTIP.RECV.DISPLAY.TYPE));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_RECV_CLEAR), LSTEXT(TOOLTIP.BUTTON.RECV.CLEAR));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_RECV_SAVE), LSTEXT(TOOLTIP.BUTTON_RECV_SAVE));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_CMB_CHANNELS), LSTEXT(TOOLTIP.CMB.CHANNELS));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_CLOSE_CHANNEL), LSTEXT(BUTTON.CLOSE.CHANNEL));
	//m_ToolTipCtrl.AddTool(GetDlgItem(IDC_SEND_DISPLAY_TYPE), L"选择发送内容的解码方式.");
	//m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_SEND_CLEAR), L"清除发送内容。");
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_SEND_FILE), LSTEXT(TOOLTIP.BUTTON.SEND.FILE));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_SEND_HISTORY), LSTEXT(TOOLTIP.BUTTON.SEND.HISTORY));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_EDIT_SEND_INTERVAL), LSTEXT(TOOLTIP.EDIT.SEND.INTERVAL));
	m_ToolTipCtrl.AddTool(GetDlgItem(IDC_BUTTON_SEND), LSTEXT(TOOLTIP.BUTTON.SEND));
	
	SetWindowText(LSTEXT(MAINWND.TITLE));

	SetControlText(IDC_STATIC_DEVICE_TYPE, LSTEXT(MAINWND.STATIC.DEVICE.TYPE));
	SetControlText(IDC_STATIC_MEMORY_MAX, LSTEXT(MAINWND.STATIC.MEMORY.MAX));
	SetControlText(IDC_STATIC_CHANNEL, LSTEXT(MAINWND.STATIC.CHANNEL));

	SetControlText(IDC_BUTTON_CONNECT, LSTEXT(MAINWND.BUTTON.CONNECT.TITLE));
	SetControlText(IDC_BUTTON_CLEAR_STATISTICS, LSTEXT(MAINWND.BUTTON.CLRSTAT.TITLE));
	SetControlText(IDC_CHECK_SHOW_RECVDATA, LSTEXT(MAINWND.CHECK.SHOW.RECVDATA));
	SetControlText(IDC_CHECK_AUTO_ADDITIONAL, LSTEXT(MAINWND.CHECK.AUTO.ADDITIONAL));
	SetControlText(IDC_CHECK_AUTO_SAVE, LSTEXT(MAINWND.CHECK.AUTO.SAVE));
	SetControlText(IDC_BUTTON_RECV_CLEAR, LSTEXT(MAINWND.BUTTON.RECV.CLEAR));
	SetControlText(IDC_BUTTON_RECV_SAVE, LSTEXT(MAINWND.BUTTON.RECV.SAVE));
	SetControlText(IDC_BUTTON_CLOSE_CHANNEL, LSTEXT(MAINWND.BUTTON.CLOSE_CHANNEL));
	SetControlText(IDC_BUTTON_SEND_FILE, LSTEXT(MAINWND.BUTTON.SEND_FILE));
	SetControlText(IDC_BUTTON_SEND_HISTORY, LSTEXT(MAINWND.BUTTON.SEND_HISTORY));
	SetControlText(IDC_BUTTON_SEND, LSTEXT(MAINWND.BUTTON.SEND));

	m_UILUpdates.push_back([this]()
	{
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.BTN.CONNECT), GetDlgItem(IDC_BUTTON_CONNECT));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.BTN.CLEARSTAT), GetDlgItem(IDC_BUTTON_CLEAR_STATISTICS));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.COMBO.MEMORY.MAX), GetDlgItem(IDC_COMBO_MEMORY_MAX));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.CHECK.SHOW.RECVDATA), GetDlgItem(IDC_CHECK_SHOW_RECVDATA));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.CHECK.AUTO.ADDITIONAL), GetDlgItem(IDC_CHECK_AUTO_ADDITIONAL));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.CHECK.AUTO.SAVE), GetDlgItem(IDC_CHECK_AUTO_SAVE));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.FILE.PATH), GetDlgItem(IDC_FILE_PATH));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.RECV.DISPLAY.TYPE), GetDlgItem(IDC_RECV_DISPLAY_TYPE));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.BUTTON.RECV.CLEAR), GetDlgItem(IDC_BUTTON_RECV_CLEAR));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.BUTTON_RECV_SAVE), GetDlgItem(IDC_BUTTON_RECV_SAVE));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.CMB.CHANNELS), GetDlgItem(IDC_CMB_CHANNELS));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(BUTTON.CLOSE.CHANNEL), GetDlgItem(IDC_BUTTON_CLOSE_CHANNEL));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.BUTTON.SEND.FILE), GetDlgItem(IDC_BUTTON_SEND_FILE));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.BUTTON.SEND.HISTORY), GetDlgItem(IDC_BUTTON_SEND_HISTORY));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.EDIT.SEND.INTERVAL), GetDlgItem(IDC_EDIT_SEND_INTERVAL));
		m_ToolTipCtrl.UpdateTipText(LSTEXT(TOOLTIP.BUTTON.SEND), GetDlgItem(IDC_BUTTON_SEND));

		SetWindowText(LSTEXT(MAINWND.TITLE));
		
		SetControlText(IDC_STATIC_DEVICE_TYPE, LSTEXT(MAINWND.STATIC.DEVICE.TYPE));
		SetControlText(IDC_STATIC_MEMORY_MAX, LSTEXT(MAINWND.STATIC.MEMORY.MAX));
		SetControlText(IDC_STATIC_CHANNEL, LSTEXT(MAINWND.STATIC.CHANNEL));

		SetControlText(IDC_BUTTON_CONNECT, LSTEXT(MAINWND.BUTTON.CONNECT.TITLE));
		SetControlText(IDC_BUTTON_CLEAR_STATISTICS, LSTEXT(MAINWND.BUTTON.CLRSTAT.TITLE));
		SetControlText(IDC_CHECK_SHOW_RECVDATA, LSTEXT(MAINWND.CHECK.SHOW.RECVDATA));
		SetControlText(IDC_CHECK_AUTO_ADDITIONAL, LSTEXT(MAINWND.CHECK.AUTO.ADDITIONAL));
		SetControlText(IDC_CHECK_AUTO_SAVE, LSTEXT(MAINWND.CHECK.AUTO.SAVE));
		SetControlText(IDC_BUTTON_RECV_CLEAR, LSTEXT(MAINWND.BUTTON.RECV.CLEAR));
		SetControlText(IDC_BUTTON_RECV_SAVE, LSTEXT(MAINWND.BUTTON.RECV.SAVE));
		SetControlText(IDC_BUTTON_CLOSE_CHANNEL, LSTEXT(MAINWND.BUTTON.CLOSE_CHANNEL));
		SetControlText(IDC_BUTTON_SEND_FILE, LSTEXT(MAINWND.BUTTON.SEND_FILE));
		SetControlText(IDC_BUTTON_SEND_HISTORY, LSTEXT(MAINWND.BUTTON.SEND_HISTORY));
		SetControlText(IDC_BUTTON_SEND, LSTEXT(MAINWND.BUTTON.SEND));

		m_SendEditor->UpdateLanguage();
	});

	LoadSendHistory();
	{
		auto types = theApp.GetDeviceTypes();
		for (auto& type : types)
		{
			auto className = std::get<0>(type);
			auto classTitle = std::get<1>(type);
			auto pClassNameBuffer = new WCHAR[className.length() + 1];
			lstrcpyn(pClassNameBuffer, className.data(), static_cast<int>(className.length() + 1));
			int item = AddComboBoxString(m_DeviceTypeCtrl, LSVT(classTitle.c_str()), pClassNameBuffer);
			m_UILUpdates.push_back([this, classTitle, item]()
			{
				UpdateComboBoxString(m_DeviceTypeCtrl, item, LSVT(classTitle.c_str()));
			});
		}

		if (!types.empty())
		{
			m_DeviceTypeCtrl.SetCurSel(theApp.GetProfileInt(L"Setting", L"DeviceType", 0));
			OnCbnSelchangeCmbNetType();
		}
	}
	
	{
		m_DevicePropertyPanel.EnableDescriptionArea();
		m_DevicePropertyPanel.SetVSDotNetLook(TRUE);
		m_DevicePropertyPanel.MarkModifiedProperties(FALSE);
		m_DevicePropertyPanel.SetAlphabeticMode(FALSE);
		m_DevicePropertyPanel.SetShowDragContext(TRUE);
		m_DevicePropertyPanel.AlwaysShowUserToolTip(TRUE);
		m_DevicePropertyPanel.EnableTrackingToolTips(TRUE);
		m_DevicePropertyPanel.EnableHeaderCtrl(FALSE);
		m_UILUpdates.push_back([this]()
		{
			m_DevicePropertyPanel.SetBoolLabels(LSTEXT(MAINWND.PROP.BOOLLABELTRUE), LSTEXT(MAINWND.PROP.BOOLLABELFALSE));
			m_DevicePropertyPanel.UpdatePropertiesTitle();
		});

		HDITEM item;
		item.cxy = 100;
		item.mask = HDI_WIDTH;
		m_DevicePropertyPanel.GetHeaderCtrl().SetItem(0, &item);
	}
	{
		InitDisplayTypeCtrl(m_RecvDisplayTypeCtrl);
	}
	m_MemoryMaxCtrl.SetCurSel(theApp.GetProfileInt(L"Setting", L"MemoryLimit", 2));
	m_AutoSaveCtrl.SetCheck(theApp.GetProfileInt(L"Setting", L"AutoSave", FALSE));
	m_bAutoSave = m_AutoSaveCtrl.GetCheck();
	m_AutoSaveFilePathCtrl.SetWindowText(theApp.GetProfileString(L"Setting", L"AutoSaveFilePath", L""));
	m_AutoSaveFilePathCtrl.EnableWindow(m_AutoSaveCtrl.GetCheck());
	m_RecvInfoAdditionalCtrl.SetCheck(theApp.GetProfileInt(L"Setting", L"LabelAdditional", FALSE));
	//m_ShowRecvDataCtrl.SetCheck(theApp.GetProfileInt(L"Setting", L"ShowRecvData", FALSE));
	m_RecvDisplayTypeCtrl.SetValue(theApp.GetProfileInt(L"Setting", L"RecvDisplayType", 0));
	m_bRecvInfoAdditional = m_RecvInfoAdditionalCtrl.GetCheck() != 0;
	//m_bShowRecvData = m_ShowRecvDataCtrl.GetCheck() == 0;

	OnCbnSelchangeComboMemoryMax();

	m_UILUpdates.push_back([this]()
	{
		m_AutoSendIntervalCtrl.Placeholder(LSTEXT(MAINWND.TXT.SEND.INTERVAL.PLACEHOLDER));
	});

	m_RecvEditCtrl.SetReadOnly(TRUE);

	CRect rect;
	GetWindowRect(rect);
	m_MinSize = rect.Size();

	UpdateUILangText();
	return TRUE;
}

void CNetDebuggerDlg::InitDisplayTypeCtrl(CSelectControl& ctrl)
{
	for (int i = 0; i < (int)TextEncodeType::_MAX; ++i)
	{
		ctrl.AddButton(TextEncodeTypeName((TextEncodeType)i), i);
	}
	ctrl.SetValue((UINT)TextEncodeType::ASCII);
}

void CNetDebuggerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	switch (nID & 0xFFF0)
	{
	case IDM_SETTING:
	{
		CSettingDlg dlgSetting;
		if (dlgSetting.DoModal() == IDOK)
		{
			theApp.WriteProfileString(L"Setting", L"LanguageId", theApp.GetLS().GetLanguage());
			UpdateUILangText();
		}
	}
	break;
	case IDM_ABOUTBOX:
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	break;
	default:
		CDialogEx::OnSysCommand(nID, lParam);
		break;
	}
}


void CNetDebuggerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CNetDebuggerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CNetDebuggerDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CDialogEx::OnGetMinMaxInfo(lpMMI);
	lpMMI->ptMinTrackSize.x = m_MinSize.cx;
	lpMMI->ptMinTrackSize.y = m_MinSize.cy;
}


void CNetDebuggerDlg::SetControlText(UINT id, const std::wstring& text)
{
	SetDlgItemText(id, text.c_str());
}

void CNetDebuggerDlg::SetControlText(UINT id, const CString& text)
{
	SetDlgItemText(id, text);
}

void CNetDebuggerDlg::SetControlEnable(UINT id, bool enable)
{
	auto hwnd = ::GetDlgItem(this->GetSafeHwnd(), id);
	::EnableWindow(hwnd, enable ? TRUE : FALSE);
	::InvalidateRect(hwnd, NULL, TRUE);
}

void CNetDebuggerDlg::OnDeviceStatusChanged(IDevice::DeviceStatus status, const std::wstring& message)
{
	auto dev = m_CDevice;
	switch (status)
	{
	case IDevice::DeviceStatus::Connecting:
	case IDevice::DeviceStatus::Disconnecting:
	{
		if (status == IDevice::DeviceStatus::Connecting)
		{
			auto listener = std::make_shared<PopWindowListener>();
			listener->OnClosing = [dev](int iReason)
			{
				if (iReason == 0)
					dev->Stop();
				return true;
			};
			m_ConnectStatusPop = PopWindow::Show(L"状态", L"正在进行连接...", PopWindow::MLOADING, 0, listener);
		}
		else
		{
			auto listener = std::make_shared<PopWindowListener>();
			listener->OnClosing = [](int iReason)
			{
				if (iReason == 0)
					return false;
				return true;
			};
			m_ConnectStatusPop = PopWindow::Show(L"状态", L"正在关闭...", PopWindow::MLOADING, 0, listener);
			m_ChannelsCtrl.ResetContent();
			if (!m_CDevice->IsSingleChannel())
			{
				AddComboBoxString(m_ChannelsCtrl, L"所有通道", nullptr);
			}
		}

		SetControlEnable(IDC_CMB_NET_TYPE, false);
		SetControlEnable(IDC_NET_DEVICE_SETTING, false);
		SetControlEnable(IDC_BUTTON_CONNECT, false);
		SetControlEnable(IDC_BUTTON_CLEAR_STATISTICS, false);
		SetControlEnable(IDC_BUTTON_SEND_FILE, false);
		SetControlEnable(IDC_SEND_EDITBOX, false);
		SetControlEnable(IDC_CMB_CHANNELS, false);
		SetControlEnable(IDC_BUTTON_SEND, false);
		SetControlEnable(IDC_BUTTON_CLOSE_CHANNEL, false);
	}
	break;
	case IDevice::DeviceStatus::Connected:
	{
		PopWindow::Close(m_ConnectStatusPop);
		m_ConnectButton.SetActive(true);
		SetControlText(IDC_BUTTON_CONNECT, LSTEXT(MAINWND.BUTTON.DECONNECT.TITLE));
		SetControlEnable(IDC_CMB_NET_TYPE, false);
		SetControlEnable(IDC_NET_DEVICE_SETTING, true);
		SetControlEnable(IDC_BUTTON_CONNECT, true);
		SetControlEnable(IDC_BUTTON_CLEAR_STATISTICS, true);
		SetControlEnable(IDC_BUTTON_SEND_FILE, true);
		SetControlEnable(IDC_SEND_EDITBOX, true);
		SetControlEnable(IDC_BUTTON_SEND, true);
		SetControlEnable(IDC_EDIT_SEND_INTERVAL, true);
		if (dev && (!dev->IsSingleChannel()))
		{
			SetControlEnable(IDC_BUTTON_CLOSE_CHANNEL, true);
			SetControlEnable(IDC_CMB_CHANNELS, true);
		}
		m_DeviceStatisticsCtrl.UpdateStatistics(true, m_ReadByteCount, m_WriteByteCount);
		SetTimer(kSTATISTICS_TIMER_ID, kSTATISTICS_UPDATE_TIME, nullptr);
		if(GetDlgItemInt(IDC_EDIT_SEND_INTERVAL)>0)
			SetTimer(kAUTO_SEND_TIMER_ID, GetDlgItemInt(IDC_EDIT_SEND_INTERVAL), nullptr);
	}
	break;
	case IDevice::DeviceStatus::Disconnected:
	{
		PopWindow::Close(m_ConnectStatusPop);
		if(!message.empty())
			PopWindow::Show(L"断开", CString(L"连接已断开:") + message.c_str(), PopWindow::MERROR, 5000);
		m_ConnectButton.SetActive(false);
		SetControlText(IDC_BUTTON_CONNECT, LSTEXT(MAINWND.BUTTON.CONNECT.TITLE));
		SetControlEnable(IDC_CMB_NET_TYPE, true);
		SetControlEnable(IDC_NET_DEVICE_SETTING, true);
		SetControlEnable(IDC_BUTTON_CONNECT, true);
		SetControlEnable(IDC_BUTTON_CLEAR_STATISTICS, false);
		SetControlEnable(IDC_BUTTON_SEND_FILE, false);
		SetControlEnable(IDC_SEND_EDITBOX, false);
		SetControlEnable(IDC_BUTTON_SEND, false);
		SetControlEnable(IDC_EDIT_SEND_INTERVAL, false);
		if (dev && (!dev->IsSingleChannel()))
		{
			SetControlEnable(IDC_BUTTON_CLOSE_CHANNEL, false);
			SetControlEnable(IDC_CMB_CHANNELS, false);
		}
		m_AutoSendIntervalCtrl.SetWindowText(L"");
		m_DeviceStatisticsCtrl.UpdateStatistics(false, m_ReadByteCount, m_WriteByteCount);
		KillTimer(kSTATISTICS_TIMER_ID);
		KillTimer(kAUTO_SEND_TIMER_ID);
		SendUIThreadTask([this]()
		{
			auto channels = ToVector(m_ChannelsMap);
			for (auto channel : channels)
			{
				channel->Close();
			}
			m_ChannelsMap.clear();
		});

		m_Closed = true;
	}
	break;
	default:
		break;
	}
	SendUIThreadTask([this]() 
	{
		if (this->GetSafeHwnd() == nullptr)
			return;
		m_DevicePropertyPanel.UpdateDeviceStatus(m_CDevice->Started());
	});
	this->UpdateWindow();
}

void CNetDebuggerDlg::OnDevicePropertyChanged(void)
{
	SendUIThreadTask([this]() {
		m_DevicePropertyPanel.UpdateProperties();
	});
}

void CNetDebuggerDlg::OnDeviceChannelConnected(std::shared_ptr<IAsyncChannel> channel, const std::wstring& message)
{
	SendUIThreadTask([this, channel]()
	{
		AddComboBoxString(m_ChannelsCtrl, channel->Description().c_str(), channel.get());
		m_ChannelsMap.insert(std::make_pair(channel->Id(), channel));
		if (m_ChannelsCtrl.GetCount() == 1)
			m_ChannelsCtrl.SetCurSel(0);
	});
	
	auto buffer = std::make_shared<std::vector<uint8_t>>();
	buffer->reserve(1024 * 8);
	buffer->resize(buffer->capacity());
	ReadChannelData(channel, buffer);
}

void CNetDebuggerDlg::OnDeviceChannelDisconnected(std::shared_ptr<IAsyncChannel> channel, const std::wstring& message)
{
	SendUIThreadTask([this, channel]()
	{
		m_ChannelsMap.erase(channel->Id());
		auto count = m_ChannelsCtrl.GetCount();
		for (int i = 0; i < count; ++i)
		{
			if (m_ChannelsCtrl.GetItemDataPtr(i) == channel.get())
			{
				m_ChannelsCtrl.DeleteItem(i);
				break;
			}
		}
	});
}

void CNetDebuggerDlg::ReadChannelData(std::shared_ptr<IAsyncChannel> channel, std::shared_ptr<std::vector<uint8_t>> buffer)
{
	auto startTime = std::chrono::high_resolution_clock::now();
	channel->ReadSome(buffer->data(), buffer->size(), [buffer, channel, startTime, this](bool ok, size_t io_bytes)
	{
		if (ok && io_bytes>0)
		{
			m_ReadByteCount += io_bytes;
			buffer->resize(io_bytes);
			AppendDataToRecvBuffer(channel, buffer);
			buffer->resize(buffer->capacity());
		}
		if(ok)
			ReadChannelData(channel, buffer);
	});
}

void CNetDebuggerDlg::AppendDataToRecvBuffer(std::shared_ptr<IAsyncChannel> channel,std::shared_ptr<std::vector<uint8_t>> buffer)
{
	{
		std::unique_lock<std::mutex> clk(m_ReadBufferMutex);
		m_ReadBuffer.Append(buffer->data(), buffer->size());
		if (m_ReadBuffer.Size() > m_MaxReadMemorySize)
		{
			if (m_bAutoSave)
			{
				CString fileName;
				m_AutoSaveFilePathCtrl.GetWindowText(fileName);
				if (fileName.GetLength() > 0)
					SaveReadHistory(fileName, false, true, false);
			}
			auto size = m_ReadBuffer.Size() - m_MaxReadMemorySize;
			m_ReadBuffer.Clear();
		}
	}

	if (m_bShowRecvData)
	{
		if (m_bRecvInfoAdditional)
		{
			auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::wstringstream ss;
			struct tm tm;
			localtime_s(&tm, &t);
			ss << std::put_time(&tm, L"%Y-%m-%d %X");
			std::wstring label = L"\r\n[";
			label += channel->RemoteEndPoint();
			label += L"] ";
			label += ss.str();
			label += L"\r\n";
			m_RecvEditCtrl.AppendLabelText(label);
		}
		m_RecvEditCtrl.AppendText(Transform::DecodeToWString(*buffer, (TextEncodeType)m_RecvDisplayTypeCtrl.GetValue()));
		auto textLen = m_RecvEditCtrl.GetTextLength();
		if (textLen > (long)m_MaxReadMemorySize)
		{
			m_RecvEditCtrl.ClearAll();
		}
	}
}

void CNetDebuggerDlg::SendUIThreadTask(std::function<void()> task)
{
	auto ptask = new std::function<void()>();
	*ptask = task;
	SendMessage(kWM_UI_THREAD_TASK, (WPARAM)ptask, (LPARAM)(~((LPARAM)ptask)));
}

void CNetDebuggerDlg::PostUIThreadTask(std::function<void()> task)
{
	auto ptask = new std::function<void()>();
	*ptask = task;
	PostMessage(kWM_UI_THREAD_TASK, (WPARAM)ptask, (LPARAM)(~((LPARAM)ptask)));
}

LRESULT CNetDebuggerDlg::OnUIThreadTask(WPARAM wParam, LPARAM lParam)
{
	if ((~wParam) != lParam || wParam==0)
		return CDialogEx::WindowProc(kWM_UI_THREAD_TASK, wParam, lParam);

	auto task = (reinterpret_cast<std::function<void()>*>(wParam));
	(*task)();
	delete task;
	return 0;
}

static CString GetCComboBoxExText(CComboBoxEx& ctrl)
{
	auto i = ctrl.GetCurSel();
	if (i < 0)
		return CString();

	TCHAR text[256] = {0};
	COMBOBOXEXITEM cbi;
	cbi.mask = CBEIF_TEXT;
	cbi.iItem = i;
	cbi.pszText = text;
	cbi.cchTextMax = sizeof(text) / sizeof(TCHAR);
	ctrl.GetItem(&cbi);
	return text;
}

static CString GetCComboBoxDataText(CComboBoxEx& ctrl)
{
	auto i = ctrl.GetCurSel();
	if (i < 0)
		return CString();
	return reinterpret_cast<WCHAR*>(ctrl.GetItemDataPtr(i));
}

static void LoadHistoryPropertyValue(const std::wstring& key, const std::wstring& prefix, IDevice::PDTable properties)
{
	for (const auto& pd : properties)
	{
		std::wstring name;
		if (!prefix.empty())
		{
			name += prefix;
			name += L".";
		}
		name += pd->Name();

		if (((int)pd->Type() & (int)IDevice::PropertyType::Group) == (int)IDevice::PropertyType::Group)
		{
			LoadHistoryPropertyValue(key, name, pd->Childs());
		}
		else
		{
			if (pd->ChangeFlags() != IDevice::PropertyChangeFlags::Readonly)
			{
				try
				{
					auto historyValue = theApp.GetProfileString(key.c_str(), name.c_str(), L"");
					pd->SetValue(historyValue.GetString());
				}
				catch (...)
				{

				}
			}
		}
	}
}

void CNetDebuggerDlg::OnCbnSelchangeCmbNetType()
{
	auto className = GetCComboBoxDataText(m_DeviceTypeCtrl);
	auto dev = theApp.CreateCommunicationDevice(className.GetString());
	m_CDevice = dev;
	m_DevicePropertyPanel.Clear();
	if (dev != nullptr)
	{
		auto properties = dev->EnumProperties();
		LoadHistoryPropertyValue((className + L".Properties").GetString(), std::wstring(), properties);
		for (const auto& pd : properties)
		{
			m_DevicePropertyPanel.AddProperty(pd);
		}
		m_DevicePropertyPanel.SetRedraw(TRUE);
		m_ChannelsCtrl.ResetContent();
		if (!dev->IsSingleChannel())
		{
			AddComboBoxString(m_ChannelsCtrl, L"所有通道", nullptr);
		}

		dev->OnStatusChanged([this](IDevice::DeviceStatus status, const std::wstring& message) 
		{
			OnDeviceStatusChanged(status, message);
		});
		dev->OnPropertyChanged([this]()
		{
			OnDevicePropertyChanged();
		});
		dev->OnChannelConnected([this](std::shared_ptr<IAsyncChannel> channel, const std::wstring& message) 
		{
			OnDeviceChannelConnected(channel, message);
		});
		dev->OnChannelDisconnected([this](std::shared_ptr<IAsyncChannel> channel, const std::wstring& message)
		{
			OnDeviceChannelDisconnected(channel, message);
		});
		m_DevicePropertyPanel.SetFocus();
	}
	else
	{
		AfxMessageBox(L"无法创建通讯设备！");
	}
	m_DevicePropertyPanel.Invalidate();
}

LRESULT CNetDebuggerDlg::OnDevicePropertyChanged(WPARAM wParam, LPARAM lParam)
{
	auto prop = reinterpret_cast<DeviceProperty*>(wParam);
	auto pd = prop->pd;
	auto value = reinterpret_cast<const wchar_t*>(lParam);
	auto dev = m_CDevice;
	if (dev != nullptr)
	{
		auto className = GetCComboBoxDataText(m_DeviceTypeCtrl);
		CString name = pd->Name().c_str();
		prop = dynamic_cast<DeviceProperty*>(prop->GetParent());
		while (prop != nullptr)
		{
			auto p = prop->pd;
			name.Insert(0, (p->Name() + L".").c_str());
			prop = dynamic_cast<DeviceProperty*>(prop->GetParent());
		}
		prop = reinterpret_cast<DeviceProperty*>(wParam);

		try
		{
			pd->SetValue(value);
			prop->UpdatePropertyValue();
			theApp.WriteProfileString(className + L".Properties", name, pd->GetValue().c_str());
		}
		catch(const std::exception& ex)
		{
			CString message(L"应用参数出错,当前参数设备可能不支持，请重新设置.\r\n");
			CStringA exMessage(ex.what());
			message += exMessage;
			PopWindow::Show(L"参数设置", message, PopWindow::MERROR, 3000);
		}
	}
	return 0;
}

void CNetDebuggerDlg::OnBnClickedButtonConnect()
{
	if (m_CDevice == nullptr)
		return;

	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);
	if (m_CDevice->Started())
	{
		m_CDevice->Stop();
	}
	else
	{
		OnBnClickedButtonClearStatistics();
		m_CDevice->Start();
	}
}

void CNetDebuggerDlg::SendDataToChannel(
	std::shared_ptr<IAsyncChannel> channel, 
	const void* buffer,
	size_t size,
	IAsyncChannel::IoCompletionHandler cphandler)
{
	auto startTime = std::chrono::high_resolution_clock::now();
	channel->Write(buffer, size, [startTime, cphandler, this](bool ok, size_t io_bytes)
	{
		if (ok)
		{
			m_WriteByteCount += io_bytes;
		}
		cphandler(ok, io_bytes);
	});
}

void CNetDebuggerDlg::SendFileBlockToChannel(std::shared_ptr<FileSendContext> ctx)
{
	if (ctx->cancel)
	{
		CString message;
		auto name = ctx->file.GetFilePath();
		message.Format(L"%s\r\n文件发送被取消.", name.GetString());
		PopWindow::Update(ctx->wid, L"发送文件", message, PopWindow::MWARNING);
		ctx->stoped = true;
		return;
	}
	ctx->dataSize = ctx->file.Read(ctx->buffer, kFILE_IO_BLOCK_SIZE);
	if (ctx->dataSize > 0)
	{
		SendDataToChannel(ctx->channel, ctx->buffer, ctx->dataSize, [ctx, this](bool ok, size_t io_bytes)
		{
			if (ok)
			{
				ctx->sentLength += io_bytes;
				if (std::chrono::system_clock::now() >= ctx->nextUpdateUI)
				{
					auto sent = (int)((ctx->sentLength * 100) / ctx->fileLength);
					CString message;
					auto name = ctx->file.GetFilePath();
					message.Format(L"%s\r\n已完成[%d]%%", name.GetString(), sent);
					PopWindow::Update(ctx->wid, L"发送文件", message, PopWindow::MNONE);
					ctx->nextUpdateUI = std::chrono::system_clock::now() + std::chrono::seconds(1);
				}
				SendFileBlockToChannel(ctx);
			}
			else
			{
				CString message;
				auto name = ctx->file.GetFilePath();
				message.Format(L"%s\r\n文件发送失败.", name.GetString());
				PopWindow::Update(ctx->wid, L"发送文件", message, PopWindow::MERROR, 5000);
				ctx->stoped = true;
			}
		});
	}
	else
	{
		CString message;
		auto name = ctx->file.GetFilePath();
		message.Format(L"%s\r\n文件发送成功.", name.GetString());
		PopWindow::Update(ctx->wid, L"发送文件", message, PopWindow::MOK, 5000);
		ctx->stoped = true;
	}
}

void CNetDebuggerDlg::StartSendFileToChannel(std::shared_ptr<IAsyncChannel> channel, const CString& fileName)
{
	auto fsctx = std::make_shared<FileSendContext>();
	CFileException ex;
	if (!fsctx->file.Open(fileName, CFile::modeRead | CFile::shareDenyWrite, &ex))
	{
		CString message;
		WCHAR tempBuffer[256];
		ex.GetErrorMessage(tempBuffer, 256);
		message.Format(L"不能打开文件[%s]\r\n%s", fileName.GetString(), tempBuffer);
		PopWindow::Show(L"错误", message, PopWindow::MERROR, 5000);
		return;
	}
	auto listener = std::make_shared<PopWindowListener>();
	listener->OnClosing = [fsctx](int iReason) 
	{
		if (fsctx->stoped)
			return true;
		fsctx->cancel = AfxMessageBox(L"是否取消文件发送?", MB_YESNO | MB_ICONQUESTION) == IDYES;
		return false;
	};

	listener->OnClosed = [fsctx]()
	{
		fsctx->cancel = true;
	};

	fsctx->wid = PopWindow::Show(L"发送文件", L"正在发送文件...", PopWindow::MLOADING, 0, listener);
	fsctx->channel = channel;
	fsctx->dataSize = 0;
	fsctx->fileLength = fsctx->file.GetLength();
	fsctx->sentLength = 0;
	fsctx->stoped = false;
	SendFileBlockToChannel(fsctx);
}

void CNetDebuggerDlg::AppendSendHistory(UINT type, std::shared_ptr<std::vector<uint8_t>> buffer)
{
	auto history = std::make_shared<SendHistoryRecord>();
	history->TextEncoding = (int)type;
	history->DataBuffer = *buffer;
	
	for (auto h : m_HistoryRecords)
	{
		if (*history == *h)
			return;
	}

	m_HistoryRecords.insert(m_HistoryRecords.begin(), history);
	if (m_HistoryRecords.size() > 64)
		m_HistoryRecords.pop_back();
}

void CNetDebuggerDlg::OnBnClickedButtonSend()
{
	if (m_ChannelsMap.empty())
	{
		PopWindow::Show(L"提示", L"当前没有可用的道通.", PopWindow::MWARNING, 3000);
		return;
	}

	auto item = m_ChannelsCtrl.GetCurSel();
	auto buffer = std::make_shared<std::vector<uint8_t>>();
	buffer->reserve(1024);
	m_SendEditor->GetDataBuffer([buffer](const uint8_t* data, size_t size)
	{
		auto offset = buffer->size();
		buffer->resize(buffer->size() + size);
		memcpy(buffer->data() + offset, data, size);
		return true;
	});

	if (buffer->empty())
		return;

	if (item >= 0 && m_ChannelsCtrl.GetItemDataPtr(item)!=nullptr)
	{
		std::shared_ptr<IAsyncChannel> channel;
		auto ptr = m_ChannelsCtrl.GetItemDataPtr(item);
		for (const auto& kv : m_ChannelsMap)
		{
			if (kv.second.get() == ptr)
			{
				channel = kv.second;
				break;
			}
		}

		if (channel != nullptr)
		{
			SendDataToChannel(channel, buffer->data(), buffer->size(), [buffer](bool,size_t) {});
		}
	}
	else
	{
		for (const auto& kv : m_ChannelsMap)
		{
			SendDataToChannel(kv.second, buffer->data(), buffer->size(), [buffer](bool, size_t) {});
		}
	}

	AppendSendHistory(m_SendEditor->GetDataType(), buffer);
}

void CNetDebuggerDlg::OnBnClickedButtonCloseChannel()
{
	auto item = m_ChannelsCtrl.GetCurSel();
	if (item >= 0 && m_ChannelsCtrl.GetItemDataPtr(item) != nullptr)
	{
		std::shared_ptr<IAsyncChannel> channel;
		auto ptr = m_ChannelsCtrl.GetItemDataPtr(item);
		for (const auto& kv : m_ChannelsMap)
		{
			if (kv.second.get() == ptr)
			{
				channel = kv.second;
				break;
			}
		}

		if (channel != nullptr)
		{
			channel->Close();
		}
	}
	else
	{
		m_ChannelsCtrl.ResetContent();
		if (!m_CDevice->IsSingleChannel())
		{
			AddComboBoxString(m_ChannelsCtrl, L"所有通道", nullptr);
		}
		auto channels = ToVector(m_ChannelsMap);
		for (auto& channel : channels)
		{
			channel->Close();
		}
	}
}

void CNetDebuggerDlg::OnBnClickedButtonSendFile()
{
	if (m_ChannelsMap.empty())
	{
		PopWindow::Show(L"提示", L"当前没有可用的道通.", PopWindow::MWARNING, 3000);
		return;
	}

	CFileDialog dlg(TRUE, nullptr, nullptr, OFN_READONLY, L"All Files (*.*)|*.*||", this);
	if (dlg.DoModal() == IDOK)
	{
		auto item = m_ChannelsCtrl.GetCurSel();
		if (item >= 0 && m_ChannelsCtrl.GetItemDataPtr(item) != nullptr)
		{
			std::shared_ptr<IAsyncChannel> channel;
			auto ptr = m_ChannelsCtrl.GetItemDataPtr(item);
			for (const auto& kv : m_ChannelsMap)
			{
				if (kv.second.get() == ptr)
				{
					channel = kv.second;
					break;
				}
			}

			if (channel != nullptr)
			{
				StartSendFileToChannel(channel, dlg.GetPathName());
			}
		}
		else
		{
			auto channels = ToVector(m_ChannelsMap);
			for (auto& channel : channels)
			{
				StartSendFileToChannel(channel, dlg.GetPathName());
			}
		}
	}
}

void CNetDebuggerDlg::OnClose()
{
	if (m_CDevice != nullptr && m_CDevice->Started())
	{
		auto ret = AfxMessageBox(L"当前设备已连接，是否关闭并退出？", MB_OKCANCEL);
		if (ret != IDOK)
			return;
		m_Closed = false;
		m_CDevice->Stop();
		theApp.GetIOContext().post([this]() 
		{
			while (!m_Closed)
				Sleep(16);

			PostUIThreadTask([this]() 
			{
				CDialogEx::OnOK();
			});
		});
	}
	else
	{
		CDialogEx::OnOK();
	}
}

void CNetDebuggerDlg::OnDestroy()
{
	SaveSendHistory();
	theApp.WriteProfileInt(L"Setting", L"MemoryLimit", m_MemoryMaxCtrl.GetCurSel());
	theApp.WriteProfileInt(L"Setting", L"AutoSave", m_AutoSaveCtrl.GetCheck());
	theApp.WriteProfileInt(L"Setting", L"LabelAdditional", m_RecvInfoAdditionalCtrl.GetCheck());
	theApp.WriteProfileInt(L"Setting", L"ShowRecvData", m_ShowRecvDataCtrl.GetCheck());
	
	CString path;
	m_AutoSaveFilePathCtrl.GetWindowText(path);
	theApp.WriteProfileString(L"Setting", L"AutoSaveFilePath", path);
	theApp.WriteProfileInt(L"Setting", L"RecvDisplayType", m_RecvDisplayTypeCtrl.GetValue());
	theApp.WriteProfileInt(L"Setting", L"DeviceType", m_DeviceTypeCtrl.GetCurSel());
	auto dev = m_CDevice;
	m_CDevice = nullptr;
	if (dev != nullptr)
	{
		dev->Stop();
		dev = nullptr;
	}
	for (int i = 0; i < m_DeviceTypeCtrl.GetCount(); ++i)
	{
		auto data = reinterpret_cast<WCHAR*>(m_DeviceTypeCtrl.GetItemDataPtr(i));
		delete[] data;
	}
	m_DeviceTypeCtrl.ResetContent();
	m_SendEditor->Destroy();
	m_SendEditor = nullptr;
	CDialogEx::OnDestroy();
}

void CNetDebuggerDlg::OnOK()
{
}

void CNetDebuggerDlg::OnCancel()
{
	OnClose();
}

void CNetDebuggerDlg::OnTimer(UINT_PTR nIDEvent)
{
	switch (nIDEvent)
	{
	case kSTATISTICS_TIMER_ID:
	{
		m_DeviceStatisticsCtrl.UpdateStatistics(true, m_ReadByteCount, m_WriteByteCount);
		m_DeviceStatisticsCtrl.RedrawWindow();
	}
	break;
	case kAUTO_SEND_TIMER_ID:
	{
		OnBnClickedButtonSend();
	}
	break;
	default:
		CDialogEx::OnTimer(nIDEvent);
		break;
	}
}

void CNetDebuggerDlg::OnBnClickedButtonClearStatistics()
{
	m_ReadByteCount = 0;
	m_WriteByteCount = 0;
	m_DeviceStatisticsCtrl.Clear();
	m_DeviceStatisticsCtrl.Invalidate();
}

void CNetDebuggerDlg::OnBnClickedButtonRecvClear()
{
	{
		std::unique_lock<std::mutex> clk(m_ReadBufferMutex);
		m_ReadBuffer.Clear();
	}
	m_RecvEditCtrl.ClearAll();
}

void CNetDebuggerDlg::SaveReadHistory(const CString& fileName, bool tip, bool append, bool lockBuffer)
{
	CFile file;
	CFileException ex;
	auto flags = CFile::modeWrite | CFile::modeCreate | CFile::shareCompat;
	if (append)
		flags |= CFile::modeNoTruncate;
	if (!file.Open(fileName, flags, &ex))
	{
		if (!tip)
			return;

		CString message;
		WCHAR tempBuffer[256];
		ex.GetErrorMessage(tempBuffer, 256);
		message.Format(L"不能打开文件[%s]\r\n%s", fileName.GetString(), tempBuffer);
		PopWindow::Show(L"错误", message, PopWindow::MERROR, 5000);
		return;
	}
	if (append)
		file.SeekToEnd();
	int tipWin = -1;
	if (tip)
		tipWin = PopWindow::Show(L"保存文件", L"正在文件文件...", PopWindow::MLOADING);
	SetControlEnable(IDC_BUTTON_RECV_CLEAR, false);
	{
		if (lockBuffer)
			m_ReadBufferMutex.lock();
		DataBufferIterator it(m_ReadBuffer);
		auto dataLength = m_ReadBuffer.Size();
		auto savedLength = 0;
		if (dataLength == 0)
			dataLength = 1;
		std::chrono::time_point<std::chrono::system_clock> nextUpdateUI = std::chrono::system_clock::now();
		do
		{
			file.Write(it.Data(), (UINT)it.Length());
			savedLength += (int)it.Length();
			if (tip && std::chrono::system_clock::now() >= nextUpdateUI)
			{
				auto saved = (int)((savedLength * 100) / dataLength);
				CString message;
				message.Format(L"已完成[%d]%%", saved);
				PopWindow::Update(tipWin, L"保存文件", message, PopWindow::MNONE);
				nextUpdateUI = std::chrono::system_clock::now() + std::chrono::seconds(1);
			}
		} while (it.Next());
		if (lockBuffer)
			m_ReadBufferMutex.unlock();
	}
	SetControlEnable(IDC_BUTTON_RECV_CLEAR, true);
	file.Close();
	if (tip)
	{
		CString message;
		message.Format(L"%s\r\n文件保存成功.", fileName.GetString());
		PopWindow::Update(tipWin, L"保存文件", message, PopWindow::MOK);
	}
}
void CNetDebuggerDlg::OnBnClickedButtonRecvSave()
{
	auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::wstringstream ss;
	struct tm tm;
	localtime_s(&tm, &now);
	ss << std::put_time(&tm, L"%F%H%M%S");
	std::wstring deffilename = ss.str();
	CFileDialog dlg(FALSE, nullptr, deffilename.c_str(), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, L"All Files (*.*)|*.*||", this);
	if (dlg.DoModal() == IDOK)
	{
		auto fileName = dlg.GetPathName();
		std::thread saveThread([this, fileName]()
		{
			SaveReadHistory(fileName, true, false, true);
		});
		saveThread.detach();
	}
}

void CNetDebuggerDlg::OnBnClickedButtonSendHistory()
{
	CSendHistoryDlg dlg(m_HistoryRecords, this);
	if (dlg.DoModal() == IDOK)
	{
		auto history = dlg.GetResult();
		if (history != nullptr)
		{
			m_SendEditor->SetDataType(history->TextEncoding);
			m_SendEditor->SetContent(history->DataBuffer.data(), history->DataBuffer.size());
		}
		else
		{
			m_SendEditor->ClearContent();
		}
	}
}


void CNetDebuggerDlg::OnCbnSelchangeComboMemoryMax()
{
	CString text;
	m_MemoryMaxCtrl.GetWindowText(text);
	LPTSTR p = nullptr;
	auto size = std::wcstoull(text.GetString(), &p, 10);
	CString u = p;
	u.Trim();
	if (u.CompareNoCase(L"KB") == 0)
		size *= 1024;
	else if (u.CompareNoCase(L"MB") == 0)
		size *= 1024 * 1024;
	else if (u.CompareNoCase(L"GB") == 0)
		size *= 1024 * 1024 * 1024;

	m_MaxReadMemorySize = (size_t)size;
}


void CNetDebuggerDlg::OnBnClickedCheckAutoSave()
{
	if (m_AutoSaveCtrl.GetCheck() != 0)
		m_AutoSaveFilePathCtrl.EnableWindow(TRUE);
	else
		m_AutoSaveFilePathCtrl.EnableWindow(FALSE);
	m_bAutoSave = m_AutoSaveCtrl.GetCheck();
}

void CNetDebuggerDlg::OnBnClickedCheckAutoAdditional()
{
	m_bRecvInfoAdditional = m_RecvInfoAdditionalCtrl.GetCheck() != 0;
}

void CNetDebuggerDlg::OnBnClickedCheckShowRecvdata()
{
	m_bShowRecvData = m_ShowRecvDataCtrl.GetCheck() == 0;
	if (!m_bShowRecvData)
	{
		PopWindow::Show(LSTEXT(POPTIP.TITLE.INFO), LSTEXT(POPTIP.BODY.HIDE_OUTPUT_WARNING), PopWindow::MINFO, 10000);
		m_RecvEditCtrl.EnableWindow(FALSE);
	}
	else
		m_RecvEditCtrl.EnableWindow(TRUE);
}


void CNetDebuggerDlg::OnEnChangeEditSendInterval()
{
	KillTimer(kAUTO_SEND_TIMER_ID);
	if (GetDlgItemInt(IDC_EDIT_SEND_INTERVAL) > 0)
		SetTimer(kAUTO_SEND_TIMER_ID, GetDlgItemInt(IDC_EDIT_SEND_INTERVAL), nullptr);
}


BOOL CNetDebuggerDlg::PreTranslateMessage(MSG* pMsg)
{
	m_ToolTipCtrl.RelayEvent(pMsg);
	return CDialogEx::PreTranslateMessage(pMsg);
}

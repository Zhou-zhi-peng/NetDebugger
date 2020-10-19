
// NetDebugger.cpp: 定义应用程序的类行为。
//

#include "pch.h"
#include "framework.h"
#include "NetDebugger.h"
#include "NetDebuggerDlg.h"
#include "CSettingDlg.h"
#include "CHelpDialog.h"
#include "ContainerWnd.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CNetDebuggerApp

BEGIN_MESSAGE_MAP(CNetDebuggerApp, CWinApp)
	ON_COMMAND(ID_HELP, &CNetDebuggerApp::OnHelp)
END_MESSAGE_MAP()


// CNetDebuggerApp 构造

CNetDebuggerApp::CNetDebuggerApp()
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的 CNetDebuggerApp 对象

CNetDebuggerApp theApp;


// CNetDebuggerApp 初始化
static void LoadLangData(HMODULE hInstance, LanguageService& ls)
{
	HRSRC res = FindResource(hInstance, MAKEINTRESOURCE(IDR_LANGUAGE), L"LANGUAGE");
	if (res)
	{
		HGLOBAL mem = LoadResource(hInstance, res);
		void *data = LockResource(mem);
		DWORD len = SizeofResource(hInstance, res);
		ls.LoadData(data, len);
		UnlockResource(mem);
		FreeResource(mem);
	}
	else
	{
		//AfxMessageBox();
	}
}

BOOL CNetDebuggerApp::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	InitContainerWnd();

	LoadLangData(m_hInstance, m_LangService);

	ULONG_PTR gdiplusToken = 0;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

	CWinApp::InitInstance();

	AfxInitRichEdit2();

	AfxEnableControlContainer();

	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager *pShellManager = new CShellManager;

	// 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	SetRegistryKey(_T("DPY"));

	auto lid = GetProfileString(L"Setting", L"LanguageId", L"");
	if (lid.IsEmpty())
	{
		CSettingDlg settingDlg;
		settingDlg.DoModal();
	}
	else
	{
		m_LangService.SetLanguage(lid);
	}
	m_LangService.SetDefaultLID(L"zh-CN");

	WriteProfileString(L"Setting", L"LanguageId", m_LangService.GetLanguage());

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	boost::asio::io_service::work work(m_IOContext);
	std::vector<std::unique_ptr<std::thread>> ioThreads;
	ioThreads.resize(si.dwNumberOfProcessors * 2);
	for (size_t i = 0; i < ioThreads.size(); ++i)
	{
		ioThreads[i].reset(new std::thread([this]() { m_IOContext.run(); }));
	}

	CNetDebuggerDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: 在此放置处理何时用
		//  “确定”来关闭对话框的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 在此放置处理何时用
		//  “取消”来关闭对话框的代码
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
	}

	m_IOContext.stop();
	for (size_t i = 0; i < ioThreads.size(); ++i)
	{
		if (ioThreads[i]->joinable())
			ioThreads[i]->join();
	}

	// 删除上面创建的 shell 管理器。
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	Gdiplus::GdiplusShutdown(gdiplusToken);
	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

std::shared_ptr<IDevice> CNetDebuggerApp::CreateCommunicationDevice(const std::wstring& className)
{
	auto it = m_CDCreatorMap.find(className);
	if (it == m_CDCreatorMap.end())
		return nullptr;
	auto crector = std::get<2>(it->second);
	return crector();
}

void CNetDebuggerApp::RegisterDeviceClass(const std::wstring& className, const std::wstring& title, FactoryCreator crector)
{
	m_CDCreatorMap.insert(std::make_pair(className, std::make_tuple(className, title, crector)));
}

CNetDebuggerApp::TypeDesc CNetDebuggerApp::GetDeviceTypes(void)
{
	TypeDesc results;
	for (auto& kv : m_CDCreatorMap)
	{
		results.push_back(std::make_tuple(std::get<0>(kv.second), std::get<1>(kv.second)));
	}
	return results;
}


void CNetDebuggerApp::OnHelp()
{
	CHelpDialog dlg;
	dlg.DoModal();
}
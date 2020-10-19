
// NetDebugger.h: PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// 主符号

#include <map>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include "LanguageService.h"

// CNetDebuggerApp:
// 有关此类的实现，请参阅 NetDebugger.cpp
//
class IDevice;
class CNetDebuggerApp : public CWinApp
{
public:
	using FactoryCreator = std::function<std::shared_ptr<IDevice>(void)>;
public:
	CNetDebuggerApp();

// 重写
public:
	virtual BOOL InitInstance();

public:
	boost::asio::io_context& GetIOContext(void) { return m_IOContext; }
public:
	using TypeDesc = std::vector<std::tuple<std::wstring, std::wstring>>;
	std::shared_ptr<IDevice> CreateCommunicationDevice(const std::wstring& className);
	void RegisterDeviceClass(const std::wstring& className, const std::wstring& title, FactoryCreator crector);
	TypeDesc GetDeviceTypes(void);
	LanguageService& GetLS(void) { return m_LangService; }
private:
	using CreatorNode = std::tuple<std::wstring, std::wstring, FactoryCreator>;
	boost::asio::io_context m_IOContext;
	std::map<std::wstring, CreatorNode> m_CDCreatorMap;
	LanguageService m_LangService;
// 实现

	DECLARE_MESSAGE_MAP()

	afx_msg void OnHelp();
};

extern CNetDebuggerApp theApp;

class AutoRegisterCDClass
{
public:
	AutoRegisterCDClass(const std::wstring& className, const std::wstring& title, CNetDebuggerApp::FactoryCreator creator)
	{
		theApp.RegisterDeviceClass(className, title, creator);
	}
	AutoRegisterCDClass(const std::wstring& className, CNetDebuggerApp::FactoryCreator creator)
	{
		theApp.RegisterDeviceClass(className, className, creator);
	}

	~AutoRegisterCDClass(void) = default;
};

#define REGISTER_CLASS(className) \
	AutoRegisterCDClass _AutoRegisterDevice_##className##_object__(	\
		L#className, \
		[](){ \
			return std::dynamic_pointer_cast<IDevice>(std::make_shared<className>()); \
		} \
	)

#define REGISTER_CLASS_TITLE(className,title) \
	AutoRegisterCDClass _AutoRegisterDevice_##className##_object__(	\
		L#className, \
		title, \
		[](){ \
			return std::dynamic_pointer_cast<IDevice>(std::make_shared<className>()); \
		} \
	)

#define LSTEXT(t) theApp.GetLS().Translate(L#t)
#define LSVT(t) theApp.GetLS().Translate(t)
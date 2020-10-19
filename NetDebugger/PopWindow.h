#pragma once
#include <afxwin.h>


class PopWindowListener;
class PopWindow
{
public:
	enum WindowType
	{
		MNONE = 0,
		MINFO = 0xE650,
		MQUESTION = 0xE616,
		MOK = 0xE61D,
		MLOADING = 0xE65D,
		MERROR = 0xE67A,
		MWARNING = 0xE966
	};
	
	static const int CLOSE_REASON_TIMEOUT = -1;
	static const int CLOSE_REASON_CLOSECMD = 0;
public:
	static int Show(
		const CString& title, 
		const CString& body,
		PopWindow::WindowType type,
		int duration = 0,
		std::shared_ptr<PopWindowListener> listener = nullptr,
		const CString* buttons = nullptr);

	static void Update(
		int windowId,
		const CString& title, 
		const CString& body,
		PopWindow::WindowType type = MNONE, 
		int duration = 0);

	static void Close(int windowId);
};

class PopWindowListener
{
public:
	std::function<void(CString& title, CString& body, PopWindow::WindowType& type, int& duration)> OnCreate;
	std::function<void(CString& title, CString& body, PopWindow::WindowType& type, int& duration)> OnUpdate;
	std::function<bool(int iReason)> OnClosing;
	std::function<void()> OnClosed;
};

#include "pch.h"
#include "ContainerWnd.h"

static LRESULT WINAPI ContainerControlWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_SIZE)
	{
		HWND subWnd = ::GetWindow(hWnd, GW_CHILD);
		CRect rect;
		GetClientRect(hWnd, &rect);
		MoveWindow(subWnd, 0, 0, rect.Width(), rect.Height(), TRUE);
	}
	return ::DefWindowProc(hWnd, Msg, wParam, lParam);
}

static BOOL RegisterWindowClass(void)
{
	static const TCHAR className[] = L"ND_CONTAINER_CONTROL";
	WNDCLASS windowclass;
	HINSTANCE hInstance = AfxGetInstanceHandle();

	if (!(::GetClassInfo(hInstance, className, &windowclass)))
	{
		windowclass.style = CS_DBLCLKS | CS_NOCLOSE;
		windowclass.lpfnWndProc = ::ContainerControlWindowProc;
		windowclass.cbClsExtra = 0;
		windowclass.cbWndExtra = sizeof(void*);
		windowclass.hInstance = hInstance;
		windowclass.hIcon = nullptr;
		windowclass.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		windowclass.hbrBackground = (HBRUSH)::GetStockObject(NULL_BRUSH);
		windowclass.lpszMenuName = nullptr;
		windowclass.lpszClassName = className;
		if (!AfxRegisterClass(&windowclass))
		{
			AfxThrowResourceException();
			return FALSE;
		}
	}
	return TRUE;
}

void InitContainerWnd(void)
{
	RegisterWindowClass();
}
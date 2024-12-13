#include <windows.h>
#include <stdio.h>
#include <dwmapi.h>
#include <wchar.h>

#include "config.h"
#include "drop.h"

static UINT WM_SHELLHOOKMESSAGE;
static WCHAR class_name[RELAY_WINDOW_CLASS_LEN];
static BOOL isDark = FALSE;

//######################################
//
//######################################
LRESULT CALLBACK DropFilesProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DROPFILES)
	{
		HWND hwnd_RELAY = FindWindowW(RELAY_WINDOW_CLASS, NULL);
		if (!hwnd_RELAY)
			return 0;

		int cnt = DragQueryFileW((HDROP)wparam, 0xFFFFFFFF, NULL, 0);

		POINT pt;
		GetCursorPos(&pt);
		HWND hwnd_target = WindowFromPoint(pt);

		for (int i = 0; i < cnt; i++)
		{
			DropData * dd = (DropData*)malloc(sizeof(DropData));
			GetWindowTextW(hwnd_target, dd->window_title, MAX_PATH);
			DragQueryFileW((HDROP)wparam, i, dd->filename, MAX_PATH);
			dd->hwnd = hwnd_target;
			dd->pt = pt;

			COPYDATASTRUCT ds;
			ds.dwData =	0;
			ds.cbData =	sizeof(DropData);
			ds.lpData =	(void *)dd;
			SendMessageW(hwnd_RELAY, WM_COPYDATA, (WPARAM)(HWND)0, (LPARAM)&ds);

			free(dd);
		}

		DragFinish((HDROP)wparam);
		return 0;
	}

	HANDLE h = GetPropW(hwnd, L"OLDPROC");
	if (h)
		return ((WNDPROC)h)(hwnd, msg, wparam, lparam);
	else
		return 0;
}

//######################################
//
//######################################
LRESULT CALLBACK NewWndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_SHELLHOOKMESSAGE)
	{
		//######################################
        // Unfortunately HSHELL_WINDOWCREATED doesn't work for X client windows,
        // so we use the first HSHELL_REDRAW notification instead
        //######################################
        if (wparam == HSHELL_REDRAW)
        {
        	HWND hwnd_new = (HWND)lparam;
			{
				GetClassNameW(hwnd_new, class_name, RELAY_WINDOW_CLASS_LEN);
				if (
					wcscmp(class_name, XWIN_CLIENT_WINDOW_CLASS1) == 0 ||
					wcscmp(class_name, XWIN_CLIENT_WINDOW_CLASS2) == 0 ||
					wcscmp(class_name, XWIN_CLIENT_WINDOW_CLASS3) == 0
				)
				{
					HANDLE h = GetPropW(hwnd_new, L"OLDPROC");
					if (!h)
					{
						//OutputDebugStringW(L"[ wsl_drag_relay.dll ] New window found.");

						// Make window title dark
						if (isDark)
						{
							INT attr = 1;
							DwmSetWindowAttribute(hwnd_new, DWMWA_USE_IMMERSIVE_DARK_MODE, &attr, sizeof(INT));
						}

						// Activate drag from explorer
						DragAcceptFiles(hwnd_new, TRUE);

						WNDPROC proc = (WNDPROC)SetWindowLongPtr(hwnd_new, GWLP_WNDPROC, (LONG_PTR)&DropFilesProc);
						SetPropW(hwnd_new, L"OLDPROC", (HANDLE)proc);
					}
				}
			}
        }
        return 0;
	}

	HANDLE h = GetPropW(hwnd, L"OLDPROC");
	if (h)
		return ((WNDPROC)h)(hwnd, msg, wparam, lparam);
	else
		return 0;
}

//######################################
//
//######################################
void DoThings()
{
	HWND hwnd;

	hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS1, NULL);
	if (!hwnd)
		hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS2, NULL);
	if (!hwnd)
		hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS3, NULL);

	if (!hwnd)
	{
		//OutputDebugStringW(L"[ wsl_drag_relay.dll ] Error: root window not found.");
		return;
	}

	RegisterShellHookWindow(hwnd);
	WM_SHELLHOOKMESSAGE = RegisterWindowMessageW(L"SHELLHOOK");

	WNDPROC old_proc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)&NewWndproc);
	SetPropW(hwnd, L"OLDPROC", (HANDLE)old_proc);

#ifdef FORCE_DARK
	isDark = TRUE;
#else
    HKEY hkey;
    if (RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize" , &hkey) == ERROR_SUCCESS)
    {
        DWORD dwData;
        DWORD dwBufSize = sizeof(DWORD);
        if (RegQueryValueExW(hkey, L"AppsUseLightTheme", 0, 0, (LPBYTE)&dwData, &dwBufSize) == ERROR_SUCCESS)
        	isDark = dwData == 0;
        RegCloseKey(hkey);
	}
#endif
}

//######################################
// For DLL mode
//######################################
extern "C" __declspec(dllexport)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			CreateThread(NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(DoThings), NULL, 0, 0);
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

//######################################
// Exporting function usable with SetWindowsHookEx
//######################################
extern "C" __declspec(dllexport)
int NextHook(int code, WPARAM wParam, LPARAM lParam)
{
	return CallNextHookEx(NULL, code, wParam, lParam);
}

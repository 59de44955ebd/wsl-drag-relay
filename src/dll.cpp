#include <windows.h>
#include <stdio.h>
#include <dwmapi.h>
#include <wchar.h>

#include "config.h"
#include "drop.h"

static UINT WM_SHELLHOOKMESSAGE;
static WCHAR class_name[RELAY_WINDOW_CLASS_LEN];
static BOOL isDark = FALSE;

BOOL isLnkFileW(WCHAR *str);
BOOL resolveLnkW(LPWSTR lpszLinkFile, LPWSTR lpszPath);

//######################################
//
//######################################
int utf16ToUtf8 (const WCHAR *utf16, char *utf8)
{
	return WideCharToMultiByte(
		CP_UTF8,
		0,
		utf16,     // source UTF-16 string
		MAX_PATH,  // total length of source UTF-16 string, in WCHARs
		utf8,	   // destination buffer
		MAX_PATH,  // size of destination buffer
		NULL,
		NULL
	);
}

//######################################
//
//######################################
void replaceChar(char *str, char orig, char rep)
{
    char *ix = str;
    while((ix = strchr(ix, orig)) != NULL)
    {
        *ix++ = rep;
    }
}

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
		if (cnt > MAX_FILES_PER_DROP)
			cnt = MAX_FILES_PER_DROP;

		DropData * dd = (DropData*)malloc(sizeof(DropData));
		GetCursorPos(&dd->pt);
		dd->hwnd = WindowFromPoint(dd->pt);

		WCHAR wwindow_title[MAX_LEN_TITLE];
		GetWindowTextW(dd->hwnd, wwindow_title, MAX_LEN_TITLE);
		utf16ToUtf8(wwindow_title, dd->window_title);

		dd->urilist[0] = 0;

		WCHAR wfilename[MAX_PATH];
		char filename[MAX_PATH];

		for (int i = 0; i < cnt; i++)
		{
			DragQueryFileW((HDROP)wparam, i, wfilename, MAX_PATH);

			if (isLnkFileW(wfilename))
			{
				WCHAR wresovled[MAX_PATH];
				resolveLnkW(wfilename, wresovled);
				wcscpy(wfilename, wresovled);
			}

			utf16ToUtf8(wfilename, filename);
			replaceChar(filename, '\\', '/');
			strcat(dd->urilist, "file://");
			char * ptr = (char *)filename;
			if (_strnicmp(filename, "//wsl.localhost/", 16) == 0)
			{
				ptr += 16;
				strcat(dd->urilist, ptr);
				strcat(dd->urilist, "\r\n");
			}
			else if (strlen(filename) > 1 && filename[1] == ':') // SMB shares not supported, they first have to be mounted in the Linux system
			{
				strcat(dd->urilist, "/mnt/");
				char drive[] = " ";
				drive[0] = tolower(filename[0]);
				strcat(dd->urilist, drive);
				ptr += 2;
				strcat(dd->urilist, ptr);
				strcat(dd->urilist, "\r\n");
			}			
		}

		DragFinish((HDROP)wparam);

		COPYDATASTRUCT ds;
		ds.dwData =	0;
		ds.cbData =	sizeof(DropData);
		ds.lpData =	(void *)dd;
		SendMessageW(hwnd_RELAY, WM_COPYDATA, (WPARAM)(HWND)0, (LPARAM)&ds);

		free(dd);

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

	// needed for .LNK file support
	CoInitialize(NULL);

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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <windows.h>
#include <objbase.h>
#include <wchar.h>
#include <X11/Xlib.h>
#include <unistd.h>

#include "config.h"
#include "drop.h"

BOOL isLnkFileW(WCHAR *str);
BOOL resolveLnkW(LPWSTR lpszLinkFile, LPWSTR lpszPath);
void createXWindow();

extern Display *disp;
extern Window wind;
extern Atom XMyDropEvent;
extern char dropped_file[MAX_PATH];
extern char window_title[MAX_PATH];

static Atom WM_PROTOCOLS, WM_DELETE_WINDOW;

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
LRESULT CALLBACK MywWndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_CREATE:
			return 0 ;

		case WM_DESTROY:
			PostQuitMessage (0) ;
			return 0 ;

		case WM_COPYDATA:
			{
				COPYDATASTRUCT * cds = (COPYDATASTRUCT*)lparam;
	            DropData * dd = (DropData*)cds->lpData;

				//printf("DROP: %ls %d\n", dd->filename, isLnkFileW(dd->filename));
				if (isLnkFileW(dd->filename))
				{
					WCHAR filename[MAX_PATH];
					resolveLnkW(dd->filename, filename);
					wcscpy(dd->filename, filename);
				}

				//######################################
				// Convert to WSL path (quick & dirty)
				//######################################
				char filename[MAX_PATH];
				utf16ToUtf8(dd->filename, filename);

				strcpy(dropped_file, "/mnt/");
				char drive[] = " ";
				drive[0] = tolower(filename[0]);
				strcat(dropped_file, drive);
				char * ptr = (char *)filename;
				ptr += 2;
				replaceChar(ptr, '\\', '/');
				strcat(dropped_file, ptr);

				utf16ToUtf8(dd->window_title, window_title);

				XEvent ev;
				//memset(&ev, 0, sizeof(XEvent));
				ev.xclient.type = ClientMessage;
				ev.xclient.window = wind;
				ev.xclient.message_type = XMyDropEvent;
				ev.xclient.format = 32;
//				ev.xclient.data.l[0] = CurrentTime;
				ev.xclient.data.l[0] = dd->pt.x;
				ev.xclient.data.l[1] = dd->pt.y;
				XSendEvent(
					disp,
					wind,
					0,
					0,
					&ev
				);
				XFlush(disp);
            }
			return 0;

		case WM_CLOSE:
			{
				XEvent ev;
				ev.xclient.type = ClientMessage;
				ev.xclient.window = wind;
				ev.xclient.message_type = XInternAtom(disp, "WM_PROTOCOLS", true);
				ev.xclient.format = 32;
				ev.xclient.data.l[0] = XInternAtom(disp, "WM_DELETE_WINDOW", false);
				ev.xclient.data.l[1] = CurrentTime;
				XSendEvent(
					disp,
					wind,
					0,
					NoEventMask,
					&ev
				);
				XFlush(disp);
			}
	}

	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

//######################################
//
//######################################
DWORD WINAPI MyThreadFunction( LPVOID lpParam )
{
	MSG msg ;

    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = MywWndproc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = 0; //hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = RELAY_WINDOW_CLASS;
    wcex.hIconSm        = 0;

    if (!RegisterClassExW(&wcex))
    {
    	printf("[ FAILED ] RegisterClassExW failed.\n");
    	return 0;
    }

    HWND hwnd = CreateWindowW(
        wcex.lpszClassName,
        L"RELAY",
        WS_OVERLAPPEDWINDOW,
        0, 0,
        300, 300,
        NULL,  // hwndParent
        NULL,
        NULL, //hInstance,
        NULL
    );

	//ShowWindow(hwnd, SW_SHOW);

	CoInitialize(NULL);

	while (GetMessage (&msg, NULL, 0, 0))
	{
		TranslateMessage (&msg) ;
		DispatchMessage (&msg) ;
	}

	return 0;
}

//######################################
//
//######################################
int main(void)
{
	// Try up to 5 sec. to find a X root window (either VcXsrv or Xming)
	HWND hwnd;
	for (int i=0; i< 50; i++)
	{
		hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS, NULL);
		if (hwnd)
			break;
		hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS2, NULL);
		if (hwnd)
			break;
		usleep(100 * 1000);
	}

	if (hwnd == NULL)
	{
		printf("[ FAILED ] Could not find target window.\n");
		return EXIT_FAILURE;
	}

	// Getting the thread of the window and the PID
	DWORD pid = 0;
	DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
	if (tid == 0)
	{
		printf("[ FAILED ] Could not get thread ID of the target window.\n");
		return EXIT_FAILURE;
	}

	// Loading DLL
	HMODULE dll = LoadLibraryExW(RELAY_DLL, NULL, DONT_RESOLVE_DLL_REFERENCES);
	if (dll == NULL)
	{
		printf("[ FAILED ] The DLL could not be found.\n");
		return EXIT_FAILURE;
	}

	// Getting exported function address
	HOOKPROC addr = (HOOKPROC)GetProcAddress(dll, "NextHook");
	if (addr == NULL)
	{
		printf("[ FAILED ] The function was not found.\n");
		return EXIT_FAILURE;
	}

	// Setting the hook in the hook chain
	HHOOK handle = SetWindowsHookEx(WH_GETMESSAGE, addr, dll, tid);
	if (handle == NULL)
	{
		printf("[ FAILED ] Couldn't set the hook with SetWindowsHookEx.\n");
		return EXIT_FAILURE;
	}

	// Triggering the hook
	PostThreadMessage(tid, WM_NULL, 0, 0);

	// Waiting for user input to remove the hook
	printf("[ OK ] Hook set and triggered.\n");

    CreateThread(
        NULL,               // default security attributes
        0,                  // use default stack size
        MyThreadFunction,   // thread function name
        0,      			// argument to thread function
        0,                  // use default creation flags
        0					// returns the thread identifier
	);

	createXWindow();
}

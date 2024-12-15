#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <wchar.h>
#include <unistd.h>

#include "config.h"
#include "drop.h"

void create_drop_window();
void send_drop_message(int x, int y, char * urilist, char * window_title);

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
			send_drop_message(dd->pt.x, dd->pt.y, dd->urilist, dd->window_title);
        }
		return 0;
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
    wcex.hInstance      = 0;
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
        NULL,
        NULL,
        NULL,
        NULL
    );

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
	// Try up to 5 sec. to find a X root window (either VcXsrv, Cygwin/X or Xming)
	HWND hwnd;
	for (int i=0; i<50; i++)
	{
		hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS1, NULL);
		if (hwnd)
			break;
		hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS2, NULL);
		if (hwnd)
			break;
		hwnd = FindWindowW(XWIN_ROOT_WINDOW_CLASS3, NULL);
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

	create_drop_window();

	// Unhooking
//	BOOL unhook = UnhookWindowsHookEx(handle);
//	if (unhook == FALSE)
//	{
//		printf("[ FAILED ] Could not remove the hook.\n");
//		return EXIT_FAILURE;
//	}
}

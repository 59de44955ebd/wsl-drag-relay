#define XWIN_ROOT_WINDOW_CLASS1 L"VcXsrv/x"
#define XWIN_CLIENT_WINDOW_CLASS1 L"vcxsrv/x X rl"

#define XWIN_ROOT_WINDOW_CLASS2 L"cygwin/x"
#define XWIN_CLIENT_WINDOW_CLASS2 L"cygwin/x X rl"

#define XWIN_ROOT_WINDOW_CLASS3 L"Xming"
#define XWIN_CLIENT_WINDOW_CLASS3 L"Xming X rl"

#define RELAY_WINDOW_CLASS L"WslDragRelayClass"
#define RELAY_WINDOW_CLASS_LEN 14
#define RELAY_DLL L"wsl-drag-relay.dll"

#define MAX_FILES_PER_DROP 10
#define MAX_PATH 260
// "file:///mnt/" + "\r\n" => 14
#define MAX_LEN_URILIST MAX_FILES_PER_DROP * (MAX_PATH + 14) + 1

#define MAX_LEN_TITLE 260

// uncomment to always force dark window title bars
//#define FORCE_DARK 1

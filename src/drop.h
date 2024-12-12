// State machine structure
typedef struct {
	WCHAR window_title[MAX_PATH];
	WCHAR filename[MAX_PATH];
	HWND hwnd;
	POINT pt;
} DropData;

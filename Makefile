all: dll exe

dll:
	rm -f dist/wsl_drag_relay.dll
	x86_64-w64-mingw32-gcc -shared -o dist/wsl_drag_relay.dll src/dll.cpp -I/usr/include -ldwmapi -L -W /lib/libX11.dll.a

exe:
	rm -f dist/wsl_drag_relay.exe
	/bin/gcc -mwindows -o dist/wsl_drag_relay.exe src/main.c src/x_window.c src/lnk.cpp -lX11 -lole32 -luuid

exe-console:
	rm -f dist/wsl_drag_relay-c.exe
	/bin/gcc -o dist/wsl_drag_relay-c.exe src/main.c src/x_window.c src/lnk.cpp -lX11 -lole32 -luuid
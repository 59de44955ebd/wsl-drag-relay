all: dll exe

dll:
	rm -f dist/wsl-drag-relay.dll
	x86_64-w64-mingw32-gcc -shared -o dist/wsl-drag-relay.dll src/lnk.cpp src/dll.cpp -I/usr/include -ldwmapi -lole32 -luuid

exe:
	rm -f dist/wsl-drag-relay.exe
	/bin/gcc -mwindows -o dist/wsl-drag-relay.exe src/main.c src/x_window.c -lxcb

exe-console:
	rm -f dist/wsl-drag-relay-c.exe
	/bin/gcc -o dist/wsl-drag-relay-c.exe src/main.c src/x_window.c -lxcb

all: exe

exe:
	rm -f dist/wsl-drag-relay.exe
	/bin/gcc -mwindows -o dist/wsl-drag-relay.exe src/main.c src/x_window.c -lxcb

exe-console:
	rm -f dist/wsl-drag-relay-c.exe
	/bin/gcc -o dist/wsl-drag-relay-c.exe src/main.c src/x_window.c -lxcb

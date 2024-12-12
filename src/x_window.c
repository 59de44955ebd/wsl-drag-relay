#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/Xlib.h>

//#include <locale.h>

#define XDND_PROTOCOL_VERSION 5
#define MAX_PATH 260

void die(const char * msg)
{
	printf("[ Error ] %s\n", msg);
	exit(EXIT_FAILURE);
}

// State structure
typedef struct {
	bool xdndExchangeStarted;
	Time xdndLastPositionTimestamp;
	int p_rootX;
	int p_rootY;
	Window otherWindow;
} XDNDState;

// shared
Display *disp;
Window wind;
Atom XMyDropEvent;
char dropped_file[MAX_PATH];
char window_title[MAX_PATH];

// private
static Atom XdndAware, XdndEnter, XdndPosition, XdndActionCopy, XdndLeave, XdndDrop,
	XdndSelection, WM_PROTOCOLS, WM_DELETE_WINDOW, typesWeAccept[6];

// XDND global state machine
static XDNDState xdndState;
static XEvent event;

// This checks if the supplied window has the XdndAware property
static int hasCorrectXdndAwareProperty(Display *disp, Window wind)
{
	// Try to get property
	int retVal = 0;
	Atom actualType = None;
	int actualFormat;
	unsigned long numOfItems, bytesAfterReturn;
	unsigned char *data = NULL;
	if (XGetWindowProperty(disp, wind, XdndAware, 0, 1024, False, AnyPropertyType,
		&actualType, &actualFormat, &numOfItems, &bytesAfterReturn, &data) == Success)
		{
		if (actualType != None)
		{
			// Assume architecture is little endian and just read first byte for XDND protocol version
			if (data[0] <= XDND_PROTOCOL_VERSION)
				retVal = data[0];
			XFree(data);
		}
	}

	return retVal;
}

// This sends the XdndEnter message which initiates the XDND protocol exchange
static void sendXdndEnter(Display *disp, int xdndVersion, Window source, Window target)
{
	// Only send if we are not already in an exchange
	if (!xdndState.xdndExchangeStarted)
	{
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndEnter;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		message.xclient.data.l[1] = xdndVersion << 24;
		message.xclient.data.l[2] = typesWeAccept[0];
		message.xclient.data.l[3] = None;
		message.xclient.data.l[4] = None;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			die("XSendEvent");
	}
}

// This sends the XdndPosition messages, which update the target on the state of the cursor
// and selected action
static void sendXdndPosition(Display *disp, Window source, Window target, int time, int p_rootX, int p_rootY)
{
	if (xdndState.xdndExchangeStarted)
	{
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndPosition;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		//message.xclient.data.l[1] reserved
		message.xclient.data.l[2] = p_rootX << 16 | p_rootY;
		message.xclient.data.l[3] = time;
		message.xclient.data.l[4] = XdndActionCopy;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			die("XSendEvent");
	}
}

// This is sent by the source when the exchange is abandoned
static void sendXdndLeave(Display *disp, Window source, Window target)
{
	if (xdndState.xdndExchangeStarted)
	{
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndLeave;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		// Rest of array members reserved so not set

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			die("XSendEvent");
	}
}

// This is sent by the source to the target to say it can call XConvertSelection
static void sendXdndDrop(Display *disp, Window source, Window target)
{
	if (xdndState.xdndExchangeStarted)
	{
		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xclient.type = ClientMessage;
		message.xclient.display = disp;
		message.xclient.window = target;
		message.xclient.message_type = XdndDrop;
		message.xclient.format = 32;
		message.xclient.data.l[0] = source;
		//message.xclient.data.l[1] reserved
		message.xclient.data.l[2] = xdndState.xdndLastPositionTimestamp;

		// Send it to target window
		if (XSendEvent(disp, target, False, 0, &message) == 0)
			die("XSendEvent");
	}
}

// This is sent by the source to the target to say the data is ready
static void sendSelectionNotify(Display *disp, XSelectionRequestEvent *selectionRequest, const char *pathStr)
{
	if (xdndState.xdndExchangeStarted)
	{
		// Allocate buffer (two bytes at end for CR/NL and another for null byte)
		size_t sizeOfPropertyData = strlen("file://") + strlen(pathStr) + 3;
		char *propertyData = malloc(sizeOfPropertyData);
		if (!propertyData)
			die("malloc");

		// Copy data to buffer
		strcpy(propertyData, "file://");
		strcat(propertyData, pathStr);
		propertyData[sizeOfPropertyData-3] = 0xD;
		propertyData[sizeOfPropertyData-2] = 0xA;
		propertyData[sizeOfPropertyData-1] = '\0';

		// Set property on target window - do not copy end null byte
		XChangeProperty(disp, selectionRequest->requestor, selectionRequest->property,
			typesWeAccept[0], 8, PropModeReplace, (unsigned char *)propertyData, sizeOfPropertyData-1);

		// Free property buffer
		free(propertyData);

		// Declare message struct and populate its values
		XEvent message;
		memset(&message, 0, sizeof(message));
		message.xselection.type = SelectionNotify;
		message.xselection.display = disp;
		message.xselection.requestor = selectionRequest->requestor;
		message.xselection.selection = selectionRequest->selection;
		message.xselection.target = selectionRequest->target;
		message.xselection.property = selectionRequest->property;
		message.xselection.time = selectionRequest->time;

		// Send it to target window
		if (XSendEvent(disp, selectionRequest->requestor, False, 0, &message) == 0)
			die("XSendEvent");

		xdndState.xdndExchangeStarted = false;
	}
}

char *getWindowName(Display *disp, Window win)
{
    Atom prop = XInternAtom(disp,"_NET_WM_NAME", False), type;
    int form;
    unsigned long remain, len;
    unsigned char * name;
    if (XGetWindowProperty(disp, win, prop, 0, 1024, False, AnyPropertyType,
                &type, &form, &len, &remain, &name) != Success)
    {
    	printf("[ FAILED ] getWindowName failed.\n");
        return NULL;
    }
    return (char*)name;
}

bool findWindow( Display *display, Window parent_window, char * window_title, Window * target_window )
{
    Window root_return;
    Window parent_return;
    Window *children_list = NULL;
    unsigned int list_length = 0;
    char *name;

    // query the window list recursively, until each window reports no sub-windows
    if ( 0 != XQueryTree( display, parent_window, &root_return, &parent_return, &children_list, &list_length ) )
    {
        if ( list_length > 0 && children_list != NULL )
        {
            for ( int i=0; i<list_length; i++)
            {
            	name = getWindowName(display, children_list[i]);
            	//printf("Window: %lu (%s)\n", children_list[i], name);
            	if (name && strcmp(name, window_title) == 0)
            	{
            		*target_window = children_list[i];
            		free(name);
            		XFree( children_list );
            		return true;
            	}
            	free(name);
            }
            XFree(children_list); // cleanup
        }
    }
    return false;
}

void do_drop(long x, long y)
{
    Window root_window = XRootWindow( disp, 0 );

    Window targetWindow;
    bool ok = findWindow( disp, root_window, window_title, &targetWindow );
    if (!ok)
    {
    	printf( "Window not found: %s\n", window_title);
    	return;
    }

	// Check targetWindow supports XDND
	int supportsXdnd = hasCorrectXdndAwareProperty(disp, targetWindow);
	if (supportsXdnd == 0)
		return;

	// Claim ownership of Xdnd selection
	XSetSelectionOwner(disp, XdndSelection, wind, event.xmotion.time);

	// Send XdndEnter message
	sendXdndEnter(disp, supportsXdnd, wind, targetWindow);
	xdndState.xdndExchangeStarted = true;
	xdndState.otherWindow = targetWindow;

	// Send XdndPosition message
	sendXdndPosition(disp, wind, targetWindow, event.xmotion.time, x, y);

	// Send XdndDrop
	sendXdndDrop(disp, wind, targetWindow);
}

// Main logic is here
void createXWindow()
{
	bool continueEventLoop = true;

	disp = XOpenDisplay(NULL);
	if (disp == NULL)
		die("XOpenDisplay");

	XMyDropEvent = XInternAtom(disp, "MY_DROP_EVENT", False);

	// Define atoms
	XdndAware = XInternAtom(disp, "XdndAware", False);
	XdndEnter = XInternAtom(disp, "XdndEnter", False);
	XdndPosition = XInternAtom(disp, "XdndPosition", False);
	XdndActionCopy = XInternAtom(disp, "XdndActionCopy", False);
	XdndLeave = XInternAtom(disp, "XdndLeave", False);
	XdndDrop = XInternAtom(disp, "XdndDrop", False);
	XdndSelection = XInternAtom(disp, "XdndSelection", False);
	WM_PROTOCOLS = XInternAtom(disp, "WM_PROTOCOLS", False);
	WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", False);

	// Define type atoms we will accept for file drop
	typesWeAccept[0] = XInternAtom(disp, "text/uri-list", False);
	typesWeAccept[1] = XInternAtom(disp, "UTF8_STRING", False);
	typesWeAccept[2] = XInternAtom(disp, "TEXT", False);
	typesWeAccept[3] = XInternAtom(disp, "STRING", False);
	typesWeAccept[4] = XInternAtom(disp, "text/plain;charset=utf-8", False);
	typesWeAccept[5] = XInternAtom(disp, "text/plain", False);

	// Get screen dimensions
	int screen = DefaultScreen(disp);

	// Create window
	wind = XCreateSimpleWindow(disp, RootWindow(disp, screen), 0, 0, 200, 200, 1, 0, 0);
	if (wind == 0)
		die("XCreateSimpleWindow");

	// Set window title
//	XStoreName(disp, wind, "XProxy");

	// Set events we are interested in
	if (XSelectInput(disp, wind, PointerMotionMask | ExposureMask) == 0)
		die("XSelectInput");

	// Set WM_PROTOCOLS to add WM_DELETE_WINDOW atom so we can end app gracefully
	XSetWMProtocols(disp, wind, &WM_DELETE_WINDOW, 1);

	// Begin listening for events
	while (continueEventLoop)
	{
		XNextEvent(disp, &event);

		switch (event.type)
		{

		// We are being asked for X selection data by the target
		case SelectionRequest:
			if (xdndState.xdndExchangeStarted) {
				//printf(">>> SelectionRequest\n");

				// Add data to the target window
				sendSelectionNotify(disp, &event.xselectionrequest, dropped_file);
			}
			break;

		// This is where we receive messages from the other window
		case ClientMessage:
			//printf("ClientMessage received %s message\n", getEventType(&event));

			if (event.xclient.message_type == XMyDropEvent)
			{
				do_drop(event.xclient.data.l[0], event.xclient.data.l[1]);
				break;
			}

			// Check if we are being closed
			if (event.xclient.message_type == WM_PROTOCOLS) {
				if (event.xclient.data.l[0] == WM_DELETE_WINDOW) {
					// End event loop
					continueEventLoop = false;
					break;
				}
			}

			break;
		}
	}

	// Destroy window and close connection
	XDestroyWindow(disp, wind);
	XCloseDisplay(disp);

	exit(0);
}

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include "config.h"

#define XDND_PROTOCOL_VERSION 5

static xcb_atom_t atmDndAware,
	atmDndEnter,
	atmDndPosition,
	atmDndActionCopy,
	atmDndDrop,
	atmDndSelection,
	atmUriListType,
	atmDropEvent;

// private
static char window_title[MAX_LEN_TITLE];
static char urilist_local[MAX_LEN_URILIST];
static int screen_num;
static xcb_connection_t * conn;
static xcb_window_t drop_window;

void send_drop_message(int x, int y, char * urilist, char * win_title)
{
	strcpy(urilist_local, urilist);
	strcpy(window_title, win_title);

    xcb_client_message_event_t e;
    memset(&e, 0, sizeof(e));
    e.response_type = XCB_CLIENT_MESSAGE;
    e.window = drop_window;
    e.type = atmDropEvent;
    e.format = 32;
    e.data.data32[0] = x;
    e.data.data32[1] = y;
    xcb_send_event(conn, 0, drop_window, XCB_EVENT_MASK_NO_EVENT, (const char *)&e);
	xcb_flush(conn);
}

static xcb_atom_t intern_atom(xcb_connection_t *conn, const char *atomName)
{
	xcb_intern_atom_reply_t *atom_reply;
	xcb_intern_atom_cookie_t atom_cookie;
	xcb_atom_t atom = XCB_ATOM_NONE;

	atom_cookie = xcb_intern_atom(conn, 0, strlen(atomName), atomName);
	atom_reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
	if (atom_reply)
	{
		atom = atom_reply->atom;
		free(atom_reply);
	}
	return atom;
}

// This checks if the supplied window has the XdndAware property
static uint32_t hasCorrectXdndAwareProperty(
	xcb_connection_t * conn,
	xcb_window_t win,
	xcb_atom_t atmDndAware
)
{
	uint32_t protocol_version = 0;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    cookie = xcb_get_property(
    	conn,
    	false,
    	win,
		atmDndAware,
		XCB_GET_PROPERTY_TYPE_ANY,
		0,
		INT_MAX
	);
    reply = xcb_get_property_reply(conn, cookie, NULL);
    if (reply && (reply->type != XCB_NONE))
    {
        protocol_version = *((uint32_t *)xcb_get_property_value(reply));
        free(reply);
    }
    return protocol_version;
}

// This sends the XdndEnter message which initiates the XDND protocol exchange
static void sendXdndEnter(
	xcb_connection_t * conn,
	int xdndVersion,
	xcb_window_t source,
	xcb_window_t target,
	xcb_atom_t atmDndEnter
//	xcb_atom_t atmType
)
{
    xcb_client_message_event_t e;
    memset(&e, 0, sizeof(e));
    e.response_type = XCB_CLIENT_MESSAGE;
    e.window = target;
    e.type = atmDndEnter;
    e.format = 32;
    e.data.data32[0] = source;
    e.data.data32[1] = xdndVersion << 24;
	e.data.data32[2] = atmUriListType;
    xcb_send_event(conn, false, target, XCB_EVENT_MASK_NO_EVENT, (const char *)&e);
}

static void sendXdndPosition(
	xcb_connection_t * conn,
	xcb_window_t source,
	xcb_window_t target,
	int time,
	int p_rootX,
	int p_rootY,
	xcb_atom_t atmDndPosition,
	xcb_atom_t atmDndActionCopy
)
{
    xcb_client_message_event_t e;
    memset(&e, 0, sizeof(e));
    e.response_type = XCB_CLIENT_MESSAGE;
    e.window = target;
    e.type = atmDndPosition;
    e.format = 32;
    e.data.data32[0] = source;
//    e.data.data32[1] reserved
	e.data.data32[2] = p_rootX << 16 | p_rootY;
	e.data.data32[3] = time;
	e.data.data32[4] = atmDndActionCopy;
    xcb_send_event(conn, false, target, XCB_EVENT_MASK_NO_EVENT, (const char *)&e);
}

// This is sent by the source to the target to say it can call XConvertSelection
static void sendXdndDrop(
	xcb_connection_t * conn,
	xcb_window_t source,
	xcb_window_t target,
	xcb_atom_t atmDndDrop,
	time_t xdndLastPositionTimestamp
)
{
    xcb_client_message_event_t e;
    memset(&e, 0, sizeof(e));
    e.response_type = XCB_CLIENT_MESSAGE;
    e.window = target;
    e.type = atmDndDrop;
    e.format = 32;
    e.data.data32[0] = source;
//    e.data.data32[1] reserved
	e.data.data32[2] = xdndLastPositionTimestamp;
    xcb_send_event(conn, false, target, XCB_EVENT_MASK_NO_EVENT, (const char *)&e);
}

// This is sent by the source to the target to say the data is ready
static void sendSelectionNotify(
	xcb_connection_t * conn,
	xcb_selection_request_event_t *selection_request
)
{
	xcb_change_property_checked(
		conn,
		XCB_PROP_MODE_REPLACE,
		selection_request->requestor,
		selection_request->property,
		atmUriListType, //typesWeAccept[0],
		8,
		strlen(urilist_local),
		(unsigned char *) urilist_local
	);

	// Setup selection notify xevent
    xcb_selection_notify_event_t eventSelection;
    eventSelection.response_type = XCB_SELECTION_NOTIFY;
    eventSelection.requestor = selection_request->requestor;
    eventSelection.selection = selection_request->selection;
    eventSelection.target = selection_request->target;
    eventSelection.property = selection_request->property;
    eventSelection.time = selection_request->time;

    xcb_send_event(
    	conn,
    	false,
    	eventSelection.requestor,
    	XCB_EVENT_MASK_NO_EVENT,
    	(const char *)&eventSelection
    );

    xcb_flush(conn);
}

static char *get_xwindow_name(
	xcb_connection_t * conn,
	xcb_window_t win
)
{
	xcb_atom_t prop = intern_atom(conn, "_NET_WM_NAME"), type;
  	xcb_get_property_cookie_t cookie = xcb_get_property(
  		conn,
		0,
		win,
		prop,
		XCB_GET_PROPERTY_TYPE_ANY,
		0,
		INT_MAX
	);

  	xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, cookie, NULL);
	if (!reply)
	{
    	printf("[ FAILED ] get_xwindow_name failed.\n");
        return NULL;
	}
    return (char*)xcb_get_property_value(reply);
}

static bool find_xwindow(
	xcb_connection_t * conn,
	xcb_window_t parent_window,
	char * window_title,
	xcb_window_t * target_window
)
{
    char *name;
	xcb_query_tree_reply_t *reply;
	xcb_query_tree_cookie_t cookie = xcb_query_tree(conn, parent_window);
	if ((reply = xcb_query_tree_reply(conn, cookie, NULL)))
	{
		xcb_window_t *children = xcb_query_tree_children(reply);
		for (int i = 0; i < xcb_query_tree_children_length(reply); i++)
		{
        	name = get_xwindow_name(conn, children[i]);
        	if (name && strcmp(name, window_title) == 0)
        	{
        		*target_window = children[i];
        		free(name);
        		free(reply);
        		return true;
        	}
        	free(name);
		}
		free(reply);
	}
    return false;
}

static xcb_screen_t *screen_of_display(
	xcb_connection_t * conn,
	int screen
)
{
	xcb_screen_iterator_t iter;
	iter = xcb_setup_roots_iterator(xcb_get_setup (conn));
	for (; iter.rem; --screen, xcb_screen_next (&iter))
	  	if (screen == 0)
	    	return iter.data;
	return NULL;
}

static void do_drop(long x, long y)
{
    // Get root screen
	xcb_screen_t * root_screen = screen_of_display(conn, screen_num);
    if (!root_screen)
    {
    	printf("[ FAILED ] Could not find root_screen.\n");
    	return;
	}

    // Find target_window
	xcb_window_t target_window;
    bool ok = find_xwindow(conn, root_screen->root, window_title, &target_window);
    if (!ok)
    {
    	printf("[ FAILED ] Could not find target_window: %s\n", window_title);
    	return;
    }

	// Check target_window supports XDND
	uint32_t xdndVersion = hasCorrectXdndAwareProperty(
		conn,
		target_window,
		atmDndAware
	);

	if (xdndVersion == 0)
		return;

	// Claim ownership of Xdnd selection
	xcb_set_selection_owner(
		conn,
		drop_window,
		atmDndSelection,
		XCB_CURRENT_TIME
	);

	// Send XdndEnter message
	sendXdndEnter(
		conn,
		xdndVersion,
		drop_window,
		target_window,
		atmDndEnter
		//typesWeAccept[0]
	);

	// Send XdndPosition message
	sendXdndPosition(
		conn,
		drop_window,
		target_window,
		XCB_CURRENT_TIME,
		x, y,
		atmDndPosition,
		atmDndActionCopy
	);

	// Send XdndDrop
	sendXdndDrop(
		conn,
		drop_window,
		target_window,
		atmDndDrop,
		XCB_CURRENT_TIME //Time xdndLastPositionTimestamp
	);

	xcb_flush(conn);
}

void create_drop_window()
{
   	xcb_screen_t * screen;
    xcb_generic_event_t * event;
    int done = 0;

	if ((conn = xcb_connect(NULL, &screen_num)) == NULL)
	{
     	printf("[ FAILED ] Could not open display.\n");
     	exit(1);
   	}

	// Define DND atoms
	atmDndAware = intern_atom(conn, "XdndAware");
	atmDndEnter = intern_atom(conn, "XdndEnter");
	atmDndPosition = intern_atom(conn, "XdndPosition");
	atmDndActionCopy = intern_atom(conn, "XdndActionCopy");
	atmDndDrop = intern_atom(conn, "XdndDrop");
	atmDndSelection = intern_atom(conn, "XdndSelection");
	atmUriListType = intern_atom(conn, "text/uri-list");
	atmDropEvent = intern_atom(conn, "MY_DROP_EVENT");

	// Get the first screen
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	drop_window = xcb_generate_id(conn);
	xcb_create_window(
		conn,
		screen->root_depth,
		drop_window,
		screen->root,
		0, 0, 100, 100,
		0, // border_width
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual,
		0,
		NULL
    );

	// uncomment to show hidden drop_window
	//xcb_map_window(conn, drop_window);

	xcb_flush(conn);

	// event loop
	while (!done && (event = xcb_wait_for_event(conn)))
	{
		switch (event->response_type & ~0x80)
		{
		case XCB_CLIENT_MESSAGE:
			xcb_client_message_event_t *client_msg = (xcb_client_message_event_t *)event;
			if (client_msg->type == atmDropEvent)
			{
				do_drop(client_msg->data.data32[0], client_msg->data.data32[1]);
				break;
			}
			break;

		// We are being asked for X selection data by the target
		case XCB_SELECTION_REQUEST:
			// Add data to the target window
			xcb_selection_request_event_t * selection_request =  (xcb_selection_request_event_t *)event;
			sendSelectionNotify(conn, selection_request);
			break;

		}
		free(event);
	}

	// Destroy window and close connection
	xcb_destroy_window(conn, drop_window);
	xcb_disconnect(conn);
}

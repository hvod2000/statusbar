#include <unistd.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <signal.h>
#include <time.h>

void error(char *message) {
    fprintf(stderr, "ERROR: %s\n", message);
    exit(1 + (((size_t)message - (size_t)error) % 255));
}

void status_time(char *buf) {
    time_t now = time(NULL);
    strftime(buf, 64, "%Y/%m/%d %H:%M:%S", localtime(&now));
}

int main()
{
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) error("can't connect to X11 server");
    void sigint_handler(int num) {
	xcb_disconnect(conn);
	error("stopped by SIGINT"); 
    }
    signal(SIGINT, sigint_handler);
    xcb_screen_t *scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    if (!scr) error("can't get root window");
    xcb_window_t root = scr->root;
    while (1) {
        static char status[128];
        status_time(status);
        xcb_change_property(conn,
            XCB_PROP_MODE_REPLACE, root, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
            8, sizeof(status), status);
        xcb_flush(conn);
        usleep(15625);
    }
}

#include <unistd.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>

void error(char *message) {
    fprintf(stderr, "ERROR: %s\n", message);
    exit(1 + (((size_t)message - (size_t)error) % 255));
}

char *cat(char *s1, char *sep, char *s2) {
    if (!s1) return s2; if (!s2) return s1;
    char *res = malloc(strlen(s1) + strlen(s2) + strlen(sep) + 1);
    sprintf(res, "%s%s%s", s1, sep, s2);
    free(s1); free(s2);
    return res;
}

char *status_time() {
    time_t now = time(NULL);
    char *status = malloc(20);
    strftime(status, 20, "%Y/%m/%d %H:%M:%S", localtime(&now));
    return status;
}

char *status_wifi() {
    char *ifaddrs2str(struct ifaddrs *ifa) {
        char ip[16], id[32] = "";
        struct iwreq req;
        strcpy(req.ifr_ifrn.ifrn_name, ifa->ifa_name);
        req.u.essid.pointer = id;
        req.u.essid.length = 32;
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        ioctl(sock, SIOCGIWESSID, &req);
        close(sock);
        getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr), ip, 16, NULL, 0, 1);
        char *res = malloc(strlen(ifa->ifa_name) + strlen(ip) + strlen(id) + 4);
        sprintf(res, "%s:%s<%s>", ifa->ifa_name, id, ip);
        return res;
    }
    struct ifaddrs *ifas;
    char *status = NULL;
    getifaddrs(&ifas);
    for (struct ifaddrs *ifa = ifas; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET
            || strcmp(ifa->ifa_name, "lo") == 0) continue;
        status = cat(status, " ", ifaddrs2str(ifa));
    }
    free(ifas);
    return status;
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
        char *status = cat(status_wifi(), " | ", status_time());
        xcb_change_property(conn,
            XCB_PROP_MODE_REPLACE, root, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
            8, strlen(status), status);
        xcb_flush(conn);
        free(status);
        usleep(15625);
    }
}

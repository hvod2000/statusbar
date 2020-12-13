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
#include <alsa/asoundlib.h>

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

char cached_volume[8] = "";
char *status_alsa(int t) {
    if (t) {
        char *status = malloc(8);
        strcpy(status, cached_volume);
        return status;
    }
    snd_mixer_t *mixer;
    if (snd_mixer_open(&mixer, 1) ||
        snd_mixer_attach(mixer, "default") ||
        snd_mixer_selem_register(mixer, NULL, NULL) ||
        snd_mixer_load(mixer)) return NULL;
    snd_mixer_selem_id_t *id;
    snd_mixer_selem_id_alloca(&id);
    snd_mixer_selem_id_set_index(id, 0);
    snd_mixer_selem_id_set_name(id, "Master");
    snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer, id);
    if (!elem) {snd_mixer_close(mixer); return NULL; };
    long min, max, vol;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &vol);
    snd_mixer_close(mixer);
    char *status = malloc(8);
    sprintf(status, "vol:%3i",((vol - min)*100 + (max - min)/2) / (max - min));
    strcpy(cached_volume, status);
    return status;
}

int main() {
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) error("can't connect to X11 server");
    void sigint_handler(int num) {
	xcb_disconnect(conn);
	error("stopped by SIGINT"); 
    }
    signal(SIGINT, sigint_handler);
    void sigalrm_handler(int num) {};
    signal(SIGALRM, sigalrm_handler);
    xcb_screen_t *scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    if (!scr) error("can't get root window");
    xcb_window_t root = scr->root;
    for (int t = 0;; t = (t + 1) % 64) {
        if (usleep(1000000)) t = 0;
        char *status = cat(
            cat(status_wifi(), " | ", status_alsa(t)),
            " | ", status_time());
        xcb_change_property(conn,
            XCB_PROP_MODE_REPLACE, root, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
            8, strlen(status), status);
        xcb_flush(conn);
        free(status);
    }
}

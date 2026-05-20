// hidread.c - minimal hidapi reader for the SInput gamepad interface.
// Opens VID 0x2E8A PID 0x10C6, usage page 0x01 / usage 0x05 (Game Pad),
// and prints any input report whose bytes differ from the previous one.
// Build: see Makefile target `hidread`.

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <hidapi.h>

#define VID 0x2E8A
#define PID 0x10C6

static volatile sig_atomic_t stop = 0;
static void on_sigint(int s) { (void)s; stop = 1; }

int main(void) {
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    if (hid_init() != 0) { fprintf(stderr, "hid_init failed\n"); return 1; }

    // Find the gamepad interface (usage page 1, usage 5) on our device.
    struct hid_device_info *devs = hid_enumerate(VID, PID);
    char path[512] = {0};
    for (struct hid_device_info *d = devs; d; d = d->next) {
        printf("iface: path=%s usage_page=0x%04hx usage=0x%04hx\n",
               d->path ? d->path : "(null)", d->usage_page, d->usage);
        if (d->usage_page == 0x01 && d->usage == 0x05 && d->path) {
            strncpy(path, d->path, sizeof(path) - 1);
        }
    }
    hid_free_enumeration(devs);

    if (!path[0]) { fprintf(stderr, "no gamepad interface (usage 1/5) found\n"); hid_exit(); return 2; }

    hid_device *h = hid_open_path(path);
    if (!h) { fprintf(stderr, "hid_open_path failed: %ls\n", hid_error(NULL)); hid_exit(); return 3; }

    printf("opened %s — reading reports (Ctrl-C to stop). Press buttons now.\n", path);
    hid_set_nonblocking(h, 1);

    uint8_t buf[64], prev[64] = {0};
    int prev_len = 0;
    while (!stop) {
        int n = hid_read_timeout(h, buf, sizeof(buf), 200);
        if (n <= 0) continue;
        if (n != prev_len || memcmp(buf, prev, n) != 0) {
            printf("report[%2d]:", n);
            for (int i = 0; i < n; i++) printf(" %02x", buf[i]);
            printf("\n");
            memcpy(prev, buf, n);
            prev_len = n;
        }
    }

    hid_close(h);
    hid_exit();
    return 0;
}

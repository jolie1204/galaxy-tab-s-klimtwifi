// SPDX-License-Identifier: MIT
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>

static int set_ipv4(int fd, unsigned long request, const char *value)
{
    struct ifreq interface = { .ifr_name = "usb0" };
    struct sockaddr_in address = { .sin_family = AF_INET };

    if (inet_pton(AF_INET, value, &address.sin_addr) != 1) {
        errno = EINVAL;
        return -1;
    }
    memcpy(&interface.ifr_addr, &address, sizeof(address));
    return ioctl(fd, request, &interface);
}

static int configure_usb(void)
{
    struct ifreq interface = { .ifr_name = "usb0" };
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    int result = ioctl(fd, SIOCGIFFLAGS, &interface);
    if (result == 0) {
        interface.ifr_flags |= IFF_UP;
        result = ioctl(fd, SIOCSIFFLAGS, &interface);
    }
    if (result == 0)
        result = set_ipv4(fd, SIOCSIFADDR, "169.254.70.2");
    if (result == 0)
        result = set_ipv4(fd, SIOCSIFNETMASK, "255.255.255.252");

    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return result;
}

int main(void)
{
    struct utsname kernel;

    if (uname(&kernel) == 0)
        fprintf(stderr, "klimtwifi probe init: %s %s\n", kernel.release, kernel.machine);

    if (mount("proc", "/proc", "proc", MS_NODEV | MS_NOSUID | MS_NOEXEC, NULL) < 0)
        perror("proc");
    if (mount("sysfs", "/sys", "sysfs", MS_NODEV | MS_NOSUID | MS_NOEXEC, NULL) < 0)
        perror("sysfs");

    for (int attempt = 0; attempt < 60; ++attempt) {
        if (configure_usb() == 0) {
            fprintf(stderr, "klimtwifi probe: usb0 at 169.254.70.2/30\n");
            break;
        }
        if (attempt == 0 || attempt == 59)
            perror("usb0");
        sleep(1);
    }

    // Keep PID 1 alive. The probe never mounts or writes an eMMC partition.
    for (;;)
        pause();
}

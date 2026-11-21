#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "uapi/ashmem.h"

static int failure(const char *operation)
{
    perror(operation);
    return 1;
}

int main(void)
{
    char name[ASHMEM_NAME_LEN] = "klimt_ashmem_probe";
    char actual[ASHMEM_NAME_LEN] = {0};
    const long page_size = sysconf(_SC_PAGESIZE);
    struct stat status;
    volatile uint32_t *writer;
    const volatile uint32_t *reader;
    int fd;

    if (page_size <= 0)
        return failure("sysconf page size");
    fd = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
    if (fd == -1)
        return failure("open ashmem");
    if (fstat(fd, &status) != 0 || !S_ISCHR(status.st_mode))
        return failure("ashmem character device");
    if (ioctl(fd, ASHMEM_SET_NAME, name) < 0 ||
        ioctl(fd, ASHMEM_GET_NAME, actual) < 0 || strcmp(name, actual) != 0)
        return failure("ashmem name");
    if (ioctl(fd, ASHMEM_SET_SIZE, (size_t)page_size) < 0 ||
        ioctl(fd, ASHMEM_GET_SIZE) != page_size)
        return failure("ashmem size");
    if (ioctl(fd, ASHMEM_SET_PROT_MASK, PROT_READ | PROT_WRITE) < 0)
        return failure("ashmem permissions");
    writer = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    reader = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (writer == MAP_FAILED || reader == MAP_FAILED)
        return failure("ashmem shared mmap");
    *writer = UINT32_C(0x3b05ca79);
    if (*reader != UINT32_C(0x3b05ca79)) {
        fputs("ashmem mappings did not share data\n", stderr);
        return 1;
    }
    printf("ASHMEM_ABI_VERIFIED bytes=%ld\n", page_size);
    return 0;
}

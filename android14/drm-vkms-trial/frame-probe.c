#include <android/log.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define TAG "KlimtFrame"

int main(int argc, char **argv)
{
    uint32_t header[4];
    uint64_t total = 0, lit = 0;
    uint32_t pixel;
    FILE *frame;

    if (argc != 2 || !(frame = fopen(argv[1], "rb")))
        return 1;
    if (fread(header, sizeof(header), 1, frame) != 1 ||
        header[0] == 0 || header[0] > 4096 ||
        header[1] == 0 || header[1] > 4096 ||
        (header[2] != 1 && header[2] != 2)) {
        fclose(frame);
        return 2;
    }

    while (fread(&pixel, sizeof(pixel), 1, frame) == 1) {
        total++;
        if (pixel & UINT32_C(0x00ffffff))
            lit++;
    }
    fclose(frame);
    if (total != (uint64_t)header[0] * header[1])
        return 3;

    __android_log_print(ANDROID_LOG_INFO, TAG, "capture=%ux%u,format=%u,lit=%" PRIu64,
                        header[0], header[1], header[2], lit);
    return lit == 0;
}

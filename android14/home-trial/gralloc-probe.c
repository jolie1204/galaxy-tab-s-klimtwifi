#include <android/hardware_buffer.h>
#include <stdint.h>
#include <stdio.h>

static int probe(uint32_t width, uint32_t height, uint64_t usage, int check_mapping) {
    AHardwareBuffer_Desc requested = {
        .width = width,
        .height = height,
        .layers = 1,
        .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        .usage = usage,
    };
    AHardwareBuffer *buffer = NULL;
    int status = AHardwareBuffer_allocate(&requested, &buffer);
    if (status) {
        printf("allocate=%d size=%ux%u\n", status, width, height);
        return status;
    }
    AHardwareBuffer_Desc actual;
    AHardwareBuffer_describe(buffer, &actual);
    if (actual.width != width || actual.height != height || actual.stride < width) {
        printf("bad geometry=%ux%u stride=%u expected=%ux%u\n", actual.width, actual.height,
               actual.stride, width, height);
        AHardwareBuffer_release(buffer);
        return -1;
    }
    if (check_mapping) {
        void *pixels = NULL;
        status = AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                      NULL, &pixels);
        if (!status) {
            *(uint32_t *)pixels = UINT32_C(0xff13579b);
            status = AHardwareBuffer_unlock(buffer, NULL);
        }
        if (!status) {
            status = AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1,
                                          NULL, &pixels);
            if (!status) {
                int matches = *(const uint32_t *)pixels == UINT32_C(0xff13579b);
                status = AHardwareBuffer_unlock(buffer, NULL);
                if (!matches) {
                    status = -1;
                }
            }
        }
        if (status) {
            printf("map=%d size=%ux%u\n", status, width, height);
            AHardwareBuffer_release(buffer);
            return status;
        }
    }
    printf("allocated=%ux%u stride=%u mapped=%d\n", width, height, actual.stride,
           check_mapping);
    AHardwareBuffer_release(buffer);
    return 0;
}

int main(void) {
    const uint64_t gpu_usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
                               AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    const uint64_t cpu_usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                               AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
    int scratch = probe(1, 1, gpu_usage | cpu_usage, 1);
    int display = probe(2560, 1600, gpu_usage | cpu_usage | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY, 0);
    return scratch || display;
}

#include <android/log.h>
#include <dlfcn.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#define TAG "KlimtVulkan"

int main(void)
{
    void *support = dlopen("libvndksupport.so", RTLD_NOW | RTLD_LOCAL);
    if (support != NULL) {
        void *(*load)(const char *, int) = dlsym(support, "android_load_sphal_library");
        if (load != NULL) {
            void *icd = load("vulkan.pastel.so", RTLD_NOW | RTLD_LOCAL);
            if (icd != NULL) {
                __android_log_print(ANDROID_LOG_INFO, TAG, "SPHAL loaded pastel");
            } else {
                const char *error = dlerror();
                __android_log_print(ANDROID_LOG_ERROR, TAG, "SPHAL pastel: %s",
                                    error ? error : "no dlerror returned");
            }
        }
    } else {
        const char *error = dlerror();
        __android_log_print(ANDROID_LOG_WARN, TAG, "libvndksupport: %s",
                            error ? error : "no dlerror returned");
    }

    void *absolute = dlopen("/vendor/lib/hw/vulkan.pastel.so", RTLD_NOW | RTLD_LOCAL);
    __android_log_print(absolute ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, TAG,
                        "Absolute pastel: %s", absolute ? "loaded" : dlerror());

    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                              .pApplicationName = TAG,
                              .apiVersion = VK_API_VERSION_1_1 };
    VkInstanceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                  .pApplicationInfo = &app };
    VkInstance instance;
    VkResult result = vkCreateInstance(&info, NULL, &instance);
    __android_log_print(ANDROID_LOG_INFO, TAG, "vkCreateInstance=%d", result);
    if (result != VK_SUCCESS)
        return 1;

    uint32_t count = 0;
    result = vkEnumeratePhysicalDevices(instance, &count, NULL);
    __android_log_print(ANDROID_LOG_INFO, TAG, "Vulkan physical devices=%u, result=%d",
                        count, result);

    if (count != 0 && result == VK_SUCCESS) {
        VkPhysicalDevice device;
        VkPhysicalDeviceProperties properties;
        count = 1;
        result = vkEnumeratePhysicalDevices(instance, &count, &device);
        if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
            vkGetPhysicalDeviceProperties(device, &properties);
            __android_log_print(ANDROID_LOG_INFO, TAG, "Vulkan renderer=%s",
                                properties.deviceName);
        }
    }
    vkDestroyInstance(instance, NULL);
    return count == 0;
}

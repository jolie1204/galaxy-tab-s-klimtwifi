#include <android/log.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define CHECK_VK(expression)                                                     \
    do {                                                                         \
        VkResult status = (expression);                                          \
        if (status != VK_SUCCESS) {                                              \
            fprintf(stderr, "%s failed with %d\n", #expression, status);          \
            exit(EXIT_FAILURE);                                                   \
        }                                                                        \
    } while (0)

struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

static struct Buffer make_buffer(VkDevice device, const VkPhysicalDeviceMemoryProperties *properties,
                                 VkDeviceSize size, VkBufferUsageFlags usage) {
    struct Buffer result = {0};
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    CHECK_VK(vkCreateBuffer(device, &info, NULL, &result.buffer));
    VkMemoryRequirements memory;
    vkGetBufferMemoryRequirements(device, result.buffer, &memory);
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < properties->memoryTypeCount; ++i) {
        if ((memory.memoryTypeBits & (1u << i)) &&
            (properties->memoryTypes[i].propertyFlags & host) == host) {
            VkMemoryAllocateInfo alloc = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memory.size,
                .memoryTypeIndex = i,
            };
            CHECK_VK(vkAllocateMemory(device, &alloc, NULL, &result.memory));
            CHECK_VK(vkBindBufferMemory(device, result.buffer, result.memory, 0));
            return result;
        }
    }
    fprintf(stderr, "No coherent host-visible Vulkan memory\n");
    exit(EXIT_FAILURE);
}

int main(void) {
    const VkDeviceSize bytes = 4096;
    const uint32_t pattern = 0x4b4c4d54;
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Klimt Vulkan queue probe",
        .apiVersion = VK_API_VERSION_1_1,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    VkInstance instance;
    CHECK_VK(vkCreateInstance(&instance_info, NULL, &instance));
    uint32_t count = 1;
    VkPhysicalDevice physical;
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &count, &physical));
    if (!count) {
        fprintf(stderr, "No Vulkan physical device\n");
        return EXIT_FAILURE;
    }
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical, &device_properties);
    if (device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU ||
        !strstr(device_properties.deviceName, "SwiftShader")) {
        fprintf(stderr, "Unexpected Vulkan device %s\n", device_properties.deviceName);
        return EXIT_FAILURE;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, NULL);
    VkQueueFamilyProperties *families = calloc(count, sizeof(*families));
    if (!families) {
        perror("queue properties");
        return EXIT_FAILURE;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families);
    uint32_t family = 0;
    while (family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        ++family;
    }
    if (family == count || families[family].queueCount < 2) {
        fprintf(stderr, "SwiftShader has fewer than two graphics queues\n");
        free(families);
        return EXIT_FAILURE;
    }
    const uint32_t queues_available = families[family].queueCount;
    free(families);

    VkPhysicalDeviceExternalSemaphoreInfo sync_fd = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
    };
    VkExternalSemaphoreProperties semaphore_properties = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
    };
    vkGetPhysicalDeviceExternalSemaphoreProperties(physical, &sync_fd, &semaphore_properties);

    float priorities[] = {1.0f, 1.0f};
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family,
        .queueCount = 2,
        .pQueuePriorities = priorities,
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
    };
    VkDevice device;
    CHECK_VK(vkCreateDevice(physical, &device_info, NULL, &device));
    VkQueue queues[2];
    vkGetDeviceQueue(device, family, 0, &queues[0]);
    vkGetDeviceQueue(device, family, 1, &queues[1]);
    if (!queues[0] || !queues[1] || queues[0] == queues[1]) {
        fprintf(stderr, "Expected distinct Vulkan queue handles\n");
        return EXIT_FAILURE;
    }

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);
    struct Buffer source = make_buffer(device, &memory_properties, bytes,
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    struct Buffer destination = make_buffer(device, &memory_properties, bytes,
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    void *mapped;
    CHECK_VK(vkMapMemory(device, destination.memory, 0, bytes, 0, &mapped));
    memset(mapped, 0, bytes);

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = family,
    };
    VkCommandPool pool;
    CHECK_VK(vkCreateCommandPool(device, &pool_info, NULL, &pool));
    VkCommandBufferAllocateInfo commands_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 2,
    };
    VkCommandBuffer commands[2];
    CHECK_VK(vkAllocateCommandBuffers(device, &commands_info, commands));
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    CHECK_VK(vkBeginCommandBuffer(commands[0], &begin));
    vkCmdFillBuffer(commands[0], source.buffer, 0, bytes, pattern);
    CHECK_VK(vkEndCommandBuffer(commands[0]));
    CHECK_VK(vkBeginCommandBuffer(commands[1], &begin));
    VkBufferCopy copy = {.size = bytes};
    vkCmdCopyBuffer(commands[1], source.buffer, destination.buffer, 1, &copy);
    VkMemoryBarrier to_host = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
    };
    vkCmdPipelineBarrier(commands[1], VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &to_host, 0, NULL, 0, NULL);
    CHECK_VK(vkEndCommandBuffer(commands[1]));

    VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkSemaphore ready;
    VkFence done;
    CHECK_VK(vkCreateSemaphore(device, &semaphore_info, NULL, &ready));
    CHECK_VK(vkCreateFence(device, &fence_info, NULL, &done));
    VkSubmitInfo fill = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commands[0],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &ready,
    };
    VkPipelineStageFlags copy_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo transfer = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ready,
        .pWaitDstStageMask = &copy_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &commands[1],
    };
    CHECK_VK(vkQueueSubmit(queues[0], 1, &fill, VK_NULL_HANDLE));
    CHECK_VK(vkQueueSubmit(queues[1], 1, &transfer, done));
    CHECK_VK(vkWaitForFences(device, 1, &done, VK_TRUE, UINT64_C(8000000000)));
    const uint32_t *words = mapped;
    for (size_t i = 0; i < bytes / sizeof(*words); ++i) {
        if (words[i] != pattern) {
            fprintf(stderr, "Queue transfer mismatch at word %zu\n", i);
            return EXIT_FAILURE;
        }
    }
    CHECK_VK(vkDeviceWaitIdle(device));
    vkUnmapMemory(device, destination.memory);
    vkDestroyFence(device, done, NULL);
    vkDestroySemaphore(device, ready, NULL);
    vkDestroyCommandPool(device, pool, NULL);
    vkDestroyBuffer(device, destination.buffer, NULL);
    vkDestroyBuffer(device, source.buffer, NULL);
    vkFreeMemory(device, destination.memory, NULL);
    vkFreeMemory(device, source.memory, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    char summary[100];
    snprintf(summary, sizeof(summary), "queues=%u distinct=1 copied=%zu sync_fd=0x%x",
             queues_available, (size_t)bytes, semaphore_properties.externalSemaphoreFeatures);
    puts(summary);
    __android_log_write(ANDROID_LOG_INFO, "KlimtQueues", summary);
    return EXIT_SUCCESS;
}

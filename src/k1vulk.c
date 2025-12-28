#include "stdio.h"
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "k1vulk.h"
#include "array.h"
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))
void *array_new(int raw_size) {
    void* pointer = malloc(raw_size);
    memset(pointer, 0, raw_size);
    return pointer;
}
#define ARRAY_NEW(type, size) array_new(sizeof(type) * (size + 1))

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const char *validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);

    return VK_FALSE;
}

static Application initWindow(int width, int height, const char *title) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    Application app = {0};
    app.window = glfwCreateWindow(width, height, title, NULL, NULL);
    return app;
}

static bool checkValidationLayerSupport() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    VkLayerProperties *availableLayers = ARRAY_NEW(VkLayerProperties, layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    bool layerFound = false;
    for (int vl_i = 0; vl_i < ARRAY_LEN(validationLayers); ++vl_i) {
        for (int lp_i = 0; lp_i < layerCount; ++lp_i) {
            if (strcmp(validationLayers[vl_i], availableLayers[lp_i].layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (layerFound) break;
    }
    free(availableLayers);
    return layerFound;
}

static void enableRequiredExtentions(VkInstanceCreateInfo *createInfo) {
    uint32_t glfwExtentionCount = 0;
    const char **glfwExtentions = glfwGetRequiredInstanceExtensions(&glfwExtentionCount);
    const char **extentions = ARRAY_NEW(const char*, glfwExtentionCount + 1);
    memcpy(extentions, glfwExtentions, sizeof(const char*) * glfwExtentionCount);
    if (enableValidationLayers) {
        extentions[glfwExtentionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }
    createInfo->enabledExtensionCount = glfwExtentionCount + 1;
    createInfo->ppEnabledExtensionNames = extentions;
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT *createInfo) {
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pfnUserCallback = debugCallback;
}

static void initVulkan(Application *app) {
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Made By K1";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "K1 Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (enableValidationLayers && !checkValidationLayerSupport()) {
        assert("validation layers requested, but not available");
    }

    enableRequiredExtentions(&createInfo);

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {0};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = ARRAY_LEN(validationLayers);
        createInfo.ppEnabledLayerNames = validationLayers;
        populateDebugMessengerCreateInfo(&debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
    }
    if (vkCreateInstance(&createInfo, NULL, &app->instance) != VK_SUCCESS) {
        assert("failed to create instance!");
    }
}

static VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void setupDebugMessenger(Application *app) {
    if (!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {0};
    populateDebugMessengerCreateInfo(&createInfo);
    if (CreateDebugUtilsMessengerEXT(app->instance, &createInfo, NULL, &app->debugMessenger) != VK_SUCCESS) {
        assert("failed to setup debug messenger!");
    }
}

static void createSurface(Application *app) {
    if (glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface) != VK_SUCCESS) {
        assert("failed to create window surface!");
    }
}

#define OPTIONAL(Type) struct {bool has_value; Type value;}
#define VECTOR_T(T) struct { T *items; int count; int capacity; }
#define VECTOR(T)   (VECTOR_T(T)){ NULL, 0, 0 }

typedef struct {
    OPTIONAL(uint32_t) graphicsFamily;
    OPTIONAL(uint32_t) presentFamily;
} QueueFamilyIndices;

static QueueFamilyIndices findQueueFamilies(Application *app, VkPhysicalDevice device) {
    QueueFamilyIndices indices = {0};
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilies = ARRAY_NEW(VkQueueFamilyProperties, queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);
    for (int i = 0; i < queueFamilyCount; i++) {
        // Present Support
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, app->surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily.value = i;
            indices.presentFamily.has_value = true;
        }
        // Graphic Support
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily.value = i;
            indices.graphicsFamily.has_value = true;
        }
    }

    return indices;
}

static bool isDeviceSuitable(Application *app, VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    // TODO: Pick the best GPU
    QueueFamilyIndices indices = findQueueFamilies(app, device);
    return indices.graphicsFamily.has_value
        && indices.presentFamily.has_value
        && deviceFeatures.geometryShader;
}

static void pickPhysicalDevice(Application *app) {
    app->physicalDevice = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(app->instance, &deviceCount, NULL);
    VkPhysicalDevice* devices = ARRAY_NEW(VkPhysicalDevice, deviceCount);
    vkEnumeratePhysicalDevices(app->instance, &deviceCount, devices);
    for (int i = 0; i < deviceCount; ++i) {
        if (isDeviceSuitable(app, devices[i])) {
            app->physicalDevice = devices[i];
            break;
        }
    }

    if (app->physicalDevice == VK_NULL_HANDLE) {
        assert("failed to find a suitable GPU!");
    }
}

static void createLogicalDevice(Application *app) {
    QueueFamilyIndices indices = findQueueFamilies(app, app->physicalDevice);

    VECTOR_T(VkDeviceQueueCreateInfo) queueCreateInfos = {0};
    uint32_t queueFamilies[2] = {
        indices.graphicsFamily.value,
        indices.presentFamily.value
    };

    float queuePriority = 1.0f;
    for(int i = 0; i < ARRAY_LEN(queueFamilies); ++i) {
        VkDeviceQueueCreateInfo queueCreateInfo = {0};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilies[i];
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        da_append(&queueCreateInfos, queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {0};
    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = queueCreateInfos.count;
    createInfo.pQueueCreateInfos = queueCreateInfos.items;

    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = ARRAY_LEN(validationLayers);
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }
    if (vkCreateDevice(app->physicalDevice, &createInfo, NULL, &app->device) != VK_SUCCESS) {
        assert("failed to create logical device!");
    }
    vkGetDeviceQueue(app->device, indices.graphicsFamily.value, 0, &app->graphicsQueue);
    vkGetDeviceQueue(app->device, indices.presentFamily.value, 0, &app->presentQueue);
    // TODO: MIGHT CAUSE PROBLEMS, I DON'T KNOW NOW!
    da_free(queueCreateInfos);
}

Application k1_init_window(int width, int height, const char *title) {
    Application app = initWindow(width, height, title);
    initVulkan(&app);
    setupDebugMessenger(&app);
    createSurface(&app);
    pickPhysicalDevice(&app);
    createLogicalDevice(&app);
    return app;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

void k1_main_loop(Application *app) {
    while(!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
    }
}

void k1_cleanup(Application *app) {
    vkDestroyDevice(app->device, NULL);
    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(app->instance, app->debugMessenger, NULL);
    }
    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    vkDestroyInstance(app->instance, NULL);
    glfwDestroyWindow(app->window);
    glfwTerminate();
}

#ifndef LIB
int main() {
    Application app = k1_init_window(800, 600, "Test");
    k1_main_loop(&app);
    k1_cleanup(&app);
    return 0;
}
#endif

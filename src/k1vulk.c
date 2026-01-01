#include "stdio.h"
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
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
void runtime_error(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

#define OPTIONAL(Type) struct {bool has_value; Type value;}
static inline int clamp(uint32_t x, uint32_t lo, uint32_t hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

typedef struct {
    char *items;
    int size;
} SizedString;

static SizedString readFile(const char* filePath) {
    SizedString str = {0};
    FILE *f = fopen(filePath, "rb");
    if (!f) {
        fprintf(stderr, "WARNING: Cannot read file (%s)!\n", filePath);
        return str;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fprintf(stderr, "WARNING: Cannot read file (%s)!\n", filePath);
        return str;
    }

    size_t size = ftell(f);
    rewind(f);

    str.items = ARRAY_NEW(char, size + 1);
    if (!str.items) {
        fclose(f);
        fprintf(stderr, "WARNING: Allocation failed (%s)!\n", filePath);
        return str;
    }

    size_t read = fread(str.items, sizeof(char), size, f);
    if (read == 0 && size != 0) {
        free(str.items);
        fclose(f);
        fprintf(stderr, "WARNING: Cannot read file (%s)!\n", filePath);
        return (SizedString){0};
    }

    str.items[read] = '\0';
    str.size = (int)read;

    fclose(f);
    return str;
}

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const int MAX_FRAMES_IN_FLIGHT = 2;

const char *validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};
const char *deviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VkVertexInputBindingDescription getVertexBindDescription() {
    VkVertexInputBindingDescription bindingDescription = {0};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

static void getVertexAttributeDescriptions(VkVertexInputAttributeDescription *attributeDescriptions, size_t numAttrDesc) {
    assert(numAttrDesc == 2 && "NUMBER OF VERTEX ATTRIBUTE DESCRIPTIONS");
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
}

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VECTOR_T(VkSurfaceFormatKHR) formats;
    VECTOR_T(VkPresentModeKHR) presentModes;
} SwapChainSupportDetails;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    (void) messageType;
    (void) pUserData;

    const char* severity;
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:  severity = "\033[33m[Warn]\033[0m"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:    severity = "\033[31m[Erro]\033[0m"; break;
        default: severity = "\033[36m[Info]\033[0m"; break;
    }

    const char* mtype;
    switch (messageType) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:             mtype = " Validation"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:            mtype = " Performance"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT: mtype = " Addr Binding"; break;
        default:                                                         mtype = ""; break;
    }

    fprintf(stderr, "%s Vulkan%s: %s\n", severity, mtype, pCallbackData->pMessage);

    return VK_FALSE;
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    (void) width;
    (void) height;
    Application *app = (Application*) glfwGetWindowUserPointer(window);
    app->framebufferResized = true;
}

static Application initWindow(int width, int height, const char *title) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    Application app = {0};
    app.window = glfwCreateWindow(width, height, title, NULL, NULL);
    app.framebufferResized = false;
    return app;
}

static bool checkValidationLayerSupport() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    VkLayerProperties *availableLayers = ARRAY_NEW(VkLayerProperties, layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    bool layerFound = false;
    for (size_t vl_i = 0; vl_i < ARRAY_LEN(validationLayers); ++vl_i) {
        for (size_t lp_i = 0; lp_i < layerCount; ++lp_i) {
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
        runtime_error("validation layers requested, but not available");
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
        runtime_error("failed to create instance!");
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
        runtime_error("failed to setup debug messenger!");
    }
}

static void createSurface(Application *app) {
    if (glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface) != VK_SUCCESS) {
        runtime_error("failed to create window surface!");
    }
}

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
    for (size_t i = 0; i < queueFamilyCount; i++) {
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

static bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

    VkExtensionProperties *availableExtensions = ARRAY_NEW(VkExtensionProperties, extensionCount);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions);

    bool extentionFound = false;
    for (size_t de_i = 0; de_i < ARRAY_LEN(deviceExtensions); ++de_i) {
        for (size_t ae_i = 0; ae_i < extensionCount; ++ae_i) {
            if (strcmp(deviceExtensions[de_i], availableExtensions[ae_i].extensionName) == 0) {
                extentionFound = true;
                break;
            }
        }
        if (extentionFound) break;
    }
    free(availableExtensions);
    return extentionFound;
}

static SwapChainSupportDetails querySwapChainSupport(Application *app, VkPhysicalDevice device) {
    SwapChainSupportDetails details = {0};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, app->surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->surface, &formatCount, NULL);
    if (formatCount != 0) {
        da_resize(&details.formats, formatCount + 1);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->surface, &formatCount, details.formats.items);
        details.formats.count = formatCount;
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->surface, &presentModeCount, NULL);
    if (presentModeCount != 0) {
        da_resize(&details.presentModes, presentModeCount + 1);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->surface, &presentModeCount, details.presentModes.items);
        details.presentModes.count = formatCount;
    }

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const VkSurfaceFormatKHR *availableFormats,
        int formatsCount) {

    for (int i = 0; i < formatsCount; ++i) {
        if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormats[i];
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(
        const VkPresentModeKHR* availablePresentModes,
        int presentModesCount) {
    for (int i = 0; i < presentModesCount; ++i) {
        if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentModes[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(Application *app, const VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(app->window, &width, &height);
        VkExtent2D actualExtent = {
            (uint32_t) width,
            (uint32_t) height
        };
        actualExtent.width  = clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

static bool isDeviceSuitable(Application *app, VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    // TODO: Pick the best GPU
    QueueFamilyIndices indices = findQueueFamilies(app, device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(app, device);
        swapChainAdequate = (swapChainSupport.formats.count > 0) && (swapChainSupport.presentModes.count > 0);
    }

    return indices.graphicsFamily.has_value
        && indices.presentFamily.has_value
        && deviceFeatures.geometryShader
        && extensionsSupported
        && swapChainAdequate;
}

static void pickPhysicalDevice(Application *app) {
    app->physicalDevice = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(app->instance, &deviceCount, NULL);
    VkPhysicalDevice* devices = ARRAY_NEW(VkPhysicalDevice, deviceCount);
    vkEnumeratePhysicalDevices(app->instance, &deviceCount, devices);
    for (size_t i = 0; i < deviceCount; ++i) {
        if (isDeviceSuitable(app, devices[i])) {
            app->physicalDevice = devices[i];
            break;
        }
    }
    if (app->physicalDevice == NULL) {
        runtime_error("failed to find a suitable GPU!");
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
    for(size_t i = 0; i < ARRAY_LEN(queueFamilies); ++i) {
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

    createInfo.enabledExtensionCount = ARRAY_LEN(deviceExtensions);
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = ARRAY_LEN(validationLayers);
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }
    if (vkCreateDevice(app->physicalDevice, &createInfo, NULL, &app->device) != VK_SUCCESS) {
        runtime_error("failed to create logical device!");
    }
    vkGetDeviceQueue(app->device, indices.graphicsFamily.value, 0, &app->graphicsQueue);
    vkGetDeviceQueue(app->device, indices.presentFamily.value, 0, &app->presentQueue);
    // TODO: MIGHT CAUSE PROBLEMS, I DON'T KNOW NOW!
    da_free(queueCreateInfos);
}

static void createSwapChain(Application *app) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(app, app->physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats.items, swapChainSupport.formats.count);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes.items, swapChainSupport.presentModes.count);
    VkExtent2D extent = chooseSwapExtent(app, swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0
            && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = app->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(app, app->physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value, indices.presentFamily.value};

    if ((indices.graphicsFamily.has_value && indices.presentFamily.has_value)
            && (indices.graphicsFamily.value != indices.presentFamily.value)) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(app->device, &createInfo, NULL, &app->swapChain) != VK_SUCCESS) {
        runtime_error("failed to create swap chain!");
    }
    vkGetSwapchainImagesKHR(app->device, app->swapChain, &imageCount, NULL);
    da_resize(&app->swapChainImages, imageCount + 1);
    vkGetSwapchainImagesKHR(app->device, app->swapChain, &imageCount, app->swapChainImages.items);
    app->swapChainImages.count = imageCount;

    app->swapChainImageFormat = surfaceFormat.format;
    app->swapChainExtent = extent;

}

static void createImageViews(Application *app) {
    da_resize(&app->swapChainImageViews, app->swapChainImages.count);
    for (size_t i = 0; i < app->swapChainImages.count; ++i) {
        VkImageViewCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = app->swapChainImages.items[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = app->swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(app->device, &createInfo, NULL, &app->swapChainImageViews.items[i]) != VK_SUCCESS) {
            runtime_error("failed to create image views!");
        }
    }
}

VkShaderModule createShaderModule(Application *app, SizedString *code) {
    VkShaderModuleCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code->size;
    createInfo.pCode = (uint32_t*) code->items;
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(app->device, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
        runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

static void createRenderPass(Application *app) {
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = app->swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {0};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(app->device, &renderPassInfo, NULL, &app->renderPass) != VK_SUCCESS) {
        runtime_error("failed to create render pass!");
    }
}

static void createGraphicsPipeline(Application *app) {
    SizedString vertShaderCode = readFile("shaders/triangle.vert.spv");
    SizedString fragShaderCode = readFile("shaders/triangle.frag.spv");
    if (vertShaderCode.size == 0 || fragShaderCode.size == 0) {
        runtime_error("Can not load nessersey shaders!");
    }
    VkShaderModule vertShaderModule = createShaderModule(app, &vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(app, &fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {0};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
    VkVertexInputBindingDescription bindingDescription = getVertexBindDescription();
    VkVertexInputAttributeDescription attributeDescriptions[2] = {0};
    getVertexAttributeDescriptions(attributeDescriptions, ARRAY_LEN(attributeDescriptions));
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = ARRAY_LEN(attributeDescriptions);
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {0};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {0};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {0};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32_t) ARRAY_LEN(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(app->device, &pipelineLayoutInfo, NULL, &app->pipelineLayout) != VK_SUCCESS) {
        runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {0};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = app->pipelineLayout;
    pipelineInfo.renderPass = app->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &app->graphicsPipeline) != VK_SUCCESS) {
        runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(app->device, fragShaderModule, NULL);
    vkDestroyShaderModule(app->device, vertShaderModule, NULL);
    free((void*) vertShaderCode.items);
    free((void*) fragShaderCode.items);
}

static void createFrameBuffers(Application *app) {
     da_resize(&app->swapChainFramebuffers, app->swapChainImageViews.count + 1);
     for (size_t i = 0; i < app->swapChainImageViews.count; i++) {
         VkImageView attachments[] = {
             app->swapChainImageViews.items[i]
         };

         VkFramebufferCreateInfo framebufferInfo = {0};
         framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         framebufferInfo.renderPass = app->renderPass;
         framebufferInfo.attachmentCount = 1;
         framebufferInfo.pAttachments = attachments;
         framebufferInfo.width = app->swapChainExtent.width;
         framebufferInfo.height = app->swapChainExtent.height;
         framebufferInfo.layers = 1;

         if (vkCreateFramebuffer(app->device, &framebufferInfo, NULL, &app->swapChainFramebuffers.items[i]) != VK_SUCCESS) {
             runtime_error("failed to create framebuffer!");
         }
         app->swapChainFramebuffers.count = app->swapChainImageViews.count;
     }
}

static void createCommandPool(Application *app) {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(app, app->physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value;
    if (vkCreateCommandPool(app->device, &poolInfo, NULL, &app->commandPool) != VK_SUCCESS) {
        runtime_error("failed to create command pool!");
    }
}

const Vertex vertices[3] = {
    { {{0.0f, -0.5f}}, {{1.0f, 1.0f, 1.0f}} },
    { {{0.5f,  0.5f}}, {{0.0f, 1.0f, 0.0f}} },
    { {{-0.5f, 0.5f}}, {{0.0f, 0.0f, 1.0f}} }
};

uint32_t findMemoryType(Application *app, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(app->physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    runtime_error("failed to find suitable memory type!");
    return 0;
}

static void createBuffer(Application *app,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer *buffer,
        VkDeviceMemory *bufferMemory) {
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(app->device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
        runtime_error("failed to create vertex buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(app->device, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(app, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(app->device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
        runtime_error("failed to allocate vertex buffer memory!");
    }
    vkBindBufferMemory(app->device, *buffer, *bufferMemory, 0);
}

static void copyBuffer(
        Application *app,
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = app->commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(app->device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
        VkBufferCopy copyRegion = {0};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(app->graphicsQueue);

    vkFreeCommandBuffers(app->device, app->commandPool, 1, &commandBuffer);
}

static void createVertexBuffer(Application *app) {
    VkDeviceSize bufferSize = sizeof(vertices);
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(
            app,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer,
            &stagingBufferMemory);
    void* data;
    vkMapMemory(app->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices, (size_t) bufferSize);
    vkUnmapMemory(app->device, stagingBufferMemory);
    createBuffer(
            app,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &app->vertexBuffer,
            &app->vertexBufferMemory);
    copyBuffer(app, stagingBuffer, app->vertexBuffer, bufferSize);
    vkDestroyBuffer(app->device, stagingBuffer, NULL);
    vkFreeMemory(app->device, stagingBufferMemory, NULL);
}

static void createCommandBuffers(Application *app) {
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = app->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    da_resize(&app->commandBuffers, (size_t) MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateCommandBuffers(app->device, &allocInfo, app->commandBuffers.items) != VK_SUCCESS) {
        runtime_error("failed to allocate command buffers!");
    }
}

static void recordCommandBuffer(Application *app, VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = NULL;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        runtime_error("failed to begin recording command buffer!");
    }
    VkRenderPassBeginInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = app->renderPass;
    renderPassInfo.framebuffer = app->swapChainFramebuffers.items[imageIndex];
    renderPassInfo.renderArea.offset = (VkOffset2D) {0, 0};
    renderPassInfo.renderArea.extent = app->swapChainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    // ---- START OF COMMAND RECORDING ---- //
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->graphicsPipeline);

        VkViewport viewport = {0};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float) app->swapChainExtent.width;
        viewport.height = (float) app->swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {0};
        scissor.offset = (VkOffset2D) {0, 0};
        scissor.extent = app->swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {app->vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdDraw(commandBuffer, ARRAY_LEN(vertices), 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    // ---- END OF COMMAND RECORDING ---- //
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        runtime_error("failed to record command buffer!");
    }
}

static void createSyncObjects(Application *app) {
    da_resize(&app->imageAvailableSemaphores, (size_t) MAX_FRAMES_IN_FLIGHT);
    da_resize(&app->renderFinishedSemaphores, (size_t) MAX_FRAMES_IN_FLIGHT);
    da_resize(&app->inFlightFences,           (size_t) MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(app->device, &semaphoreInfo, NULL, &app->imageAvailableSemaphores.items[i])
                != VK_SUCCESS ||
                vkCreateSemaphore(app->device, &semaphoreInfo, NULL, &app->renderFinishedSemaphores.items[i])
                != VK_SUCCESS ||
                vkCreateFence(app->device, &fenceInfo, NULL, &app->inFlightFences.items[i])
                != VK_SUCCESS) {
            runtime_error("failed to create semaphores!");
        }
    }
}

static void cleanupSwapChain(Application *app) {
    for (size_t i = 0; i < app->swapChainFramebuffers.count; ++i) {
        vkDestroyFramebuffer(app->device, app->swapChainFramebuffers.items[i], NULL);
    }
    for (size_t i = 0; i < app->swapChainImageViews.count; ++i) {
        vkDestroyImageView(app->device, app->swapChainImageViews.items[i], NULL);
    }
    vkDestroySwapchainKHR(app->device, app->swapChain, NULL);
}

static void recreateSwapChain(Application *app) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(app->window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(app->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(app->device);

    cleanupSwapChain(app);

    createSwapChain(app);
    createImageViews(app);
    createFrameBuffers(app);
}

static void setGlfwResizeWindowCallbacks(Application *app) {
    glfwSetWindowUserPointer(app->window, app);
    glfwSetFramebufferSizeCallback(app->window, framebufferResizeCallback);
}

Application k1_init_window(int width, int height, const char *title) {
    Application app = initWindow(width, height, title);
    setGlfwResizeWindowCallbacks(&app);
    initVulkan(&app);
    setupDebugMessenger(&app);
    createSurface(&app);
    pickPhysicalDevice(&app);
    createLogicalDevice(&app);

    createSwapChain(&app);
    createImageViews(&app);
    createRenderPass(&app);
    createGraphicsPipeline(&app);
    createFrameBuffers(&app);
    createCommandPool(&app);
    createVertexBuffer(&app);
    createCommandBuffers(&app);
    createSyncObjects(&app);
    return app;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

/* *
 * Wait for the previous frame to finish
 * Acquire an image from the swap chain
 * Record a command buffer which draws the scene onto that image
 * Submit the recorded command buffer
 * Present the swap chain image
 * */
static size_t currentFrame = 0;
static void drawFrame(Application *app) {
    vkWaitForFences(app->device, 1, &(app->inFlightFences.items[currentFrame]), VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(app->device, app->swapChain, UINT64_MAX,
            app->imageAvailableSemaphores.items[currentFrame],
            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain(app);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(app->device, 1, &app->inFlightFences.items[currentFrame]);

    vkResetCommandBuffer(app->commandBuffers.items[currentFrame], 0);
    recordCommandBuffer(app, app->commandBuffers.items[currentFrame], imageIndex);

    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {app->imageAvailableSemaphores.items[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &app->commandBuffers.items[currentFrame];

    VkSemaphore signalSemaphores[] = {app->renderFinishedSemaphores.items[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, app->inFlightFences.items[currentFrame]) != VK_SUCCESS) {
        runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {app->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(app->presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || app->framebufferResized) {
        app->framebufferResized = false;
        recreateSwapChain(app);
    } else if (result != VK_SUCCESS) {
        runtime_error("failed to present swap chain image!");
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void k1_main_loop(Application *app) {
    while(!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
        drawFrame(app);
    }
    vkDeviceWaitIdle(app->device);
}

void k1_cleanup(Application *app) {
    cleanupSwapChain(app);
    vkDestroyBuffer(app->device, app->vertexBuffer, NULL);
    vkFreeMemory(app->device, app->vertexBufferMemory, NULL);
    vkDestroyPipeline(app->device, app->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipelineLayout, NULL);
    vkDestroyRenderPass(app->device, app->renderPass, NULL);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(app->device, app->renderFinishedSemaphores.items[i], NULL);
        vkDestroySemaphore(app->device, app->imageAvailableSemaphores.items[i], NULL);
        vkDestroyFence(app->device, app->inFlightFences.items[i], NULL);
    }
    vkDestroyCommandPool(app->device, app->commandPool, NULL);
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

#ifndef K1VULK_H
#define K1VULK_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VECTOR_T(T) struct { T *items; int count; int capacity; }

typedef struct {
    GLFWwindow *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    VECTOR_T(VkImage) swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VECTOR_T(VkImageView) swapChainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VECTOR_T(VkFramebuffer) swapChainFramebuffers;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
} Application;

Application k1_init_window(int width, int height, const char *title);
void k1_main_loop(Application *app);
void k1_cleanup(Application *app);
#endif

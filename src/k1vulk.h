#ifndef K1VULK_H
#define K1VULK_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
} Application;

Application k1_init_window(int width, int height, const char *title);
void k1_main_loop(Application *app);
void k1_cleanup(Application *app);
#endif

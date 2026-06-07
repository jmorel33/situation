#include <stdio.h>
#include <vulkan/vulkan.h>

int main(void) {
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "probe", 1, "probe", 1, VK_API_VERSION_1_4 };
    VkInstanceCreateInfo ci = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL };
    VkInstance inst;
    if (vkCreateInstance(&ci, NULL, &inst) != VK_SUCCESS) return 1;

    uint32_t pd_count = 0;
    vkEnumeratePhysicalDevices(inst, &pd_count, NULL);
    VkPhysicalDevice pd[8];
    if (pd_count > 8) pd_count = 8;
    vkEnumeratePhysicalDevices(inst, &pd_count, pd);

    for (uint32_t i = 0; i < pd_count; ++i) {
        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dyn3 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT };
        VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &dyn3 };
        vkGetPhysicalDeviceFeatures2(pd[i], &f2);
        printf("GPU %u: fillModeNonSolid=%d extendedDynamicState3PolygonMode=%d\n",
            i, f2.features.fillModeNonSolid, dyn3.extendedDynamicState3PolygonMode);
    }

    vkDestroyInstance(inst, NULL);
    return 0;
}

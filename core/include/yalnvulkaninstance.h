#ifndef YALN_VULKAN_INSTANCE_H
#define YALN_VULKAN_INSTANCE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

#include "global_type.h"

// ============================================================================
//  YalnVulkanInstance
//  职责：Vulkan 实例（VkInstance）、校验层开关、调试消息信使。
//        不涉及任何设备/交换链/渲染逻辑。
// ============================================================================

// 编译期决定是否启用校验层：Debug 构建开启，Release 关闭
#ifdef NDEBUG
constexpr bool YALN_ENABLE_VALIDATION_LAYER = false;
#else
constexpr bool YALN_ENABLE_VALIDATION_LAYER = true;
#endif

class YALN_CORE_EXPORT YalnVulkanInstance
{
public:
    YalnVulkanInstance() = default;
    ~YalnVulkanInstance() = default;

    YalnVulkanInstance(const YalnVulkanInstance&)            = delete;
    YalnVulkanInstance& operator=(const YalnVulkanInstance&) = delete;

    // 创建实例。extensions 由调用方（视图层）提供实例级扩展
    bool create(const char* applicationName, const std::vector<const char*>& extensions);

    const vk::raii::Context&  context()  const { return m_context; }
    const vk::raii::Instance& handle()   const { return m_instance; }
    bool isValid()                       const { return m_instance != nullptr; }
    bool validationEnabled()             const { return m_validationEnabled; }

    // 校验层相关（设备层创建逻辑设备时也会用到）
    static bool validationLayerEnabled() { return YALN_ENABLE_VALIDATION_LAYER; }
    static const std::vector<const char*>& validationLayers();
    bool checkValidationLayerSupport() const;

    // 调试回调
    static vk::DebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo();
    static VKAPI_ATTR uint32_t VKAPI_CALL debugMessageFunc(
        vk::DebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT              messageTypes,
        vk::DebugUtilsMessengerCallbackDataEXT const * pCallbackData,
        void *                                         pUserData);

private:
    void setupDebugMessenger();

    vk::raii::Context               m_context;
    vk::raii::Instance              m_instance{ nullptr };
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger{ nullptr };

    bool m_validationEnabled = YALN_ENABLE_VALIDATION_LAYER;
};

#endif // YALN_VULKAN_INSTANCE_H

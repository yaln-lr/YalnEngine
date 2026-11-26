#include "../include/yalnvulkaninstance.h"

#include <iostream>
#include <set>
#include <string>

const std::vector<const char*>& YalnVulkanInstance::validationLayers()
{
    static const std::vector<const char*> layers{ "VK_LAYER_KHRONOS_validation" };
    return layers;
}

VKAPI_ATTR uint32_t VKAPI_CALL YalnVulkanInstance::debugMessageFunc(
    vk::DebugUtilsMessageSeverityFlagBitsEXT       /*messageSeverity*/,
    vk::DebugUtilsMessageTypeFlagsEXT              /*messageTypes*/,
    vk::DebugUtilsMessengerCallbackDataEXT const * pCallbackData,
    void *                                         /*pUserData*/)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT YalnVulkanInstance::populateDebugMessengerCreateInfo()
{
    constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    return { {}, severityFlags, messageTypeFlags, &debugMessageFunc };
}

bool YalnVulkanInstance::checkValidationLayerSupport() const
{
    const auto availableLayers = m_context.enumerateInstanceLayerProperties();

    std::set<std::string> requiredLayers;
    for (const char* name : validationLayers())
    {
        requiredLayers.emplace(name);
    }

    for (const auto& layer : availableLayers)
    {
        requiredLayers.erase(layer.layerName);
    }

    return requiredLayers.empty();
}

bool YalnVulkanInstance::create(std::string_view applicationName,
                                const std::vector<const char*>& extensions)
{
    if (YALN_ENABLE_VALIDATION_LAYER && !checkValidationLayerSupport())
    {
        std::cerr << "Validation layer requested, but not available!" << std::endl;
        return false;
    }

    // string_view 不保证 null 结尾，Vulkan C API 需要 C 字符串，局部拷贝一份。
    const std::string appName(applicationName);
    vk::ApplicationInfo appInfo(
        appName.c_str(), 1,
        "YalnEngine", 1,
        VK_API_VERSION_1_0);

    const auto& layers = validationLayers();

    vk::InstanceCreateInfo createInfo(
        {},
        &appInfo,
        YALN_ENABLE_VALIDATION_LAYER ? static_cast<uint32_t>(layers.size()) : 0u,
        layers.data(),
        static_cast<uint32_t>(extensions.size()),
        extensions.data());

    auto debugCreateInfo = populateDebugMessengerCreateInfo();
    if (YALN_ENABLE_VALIDATION_LAYER)
    {
        createInfo.pNext = &debugCreateInfo;
    }

    try
    {
        m_instance = vk::raii::Instance(m_context, createInfo);
    }
    catch (vk::SystemError& e)
    {
        std::cerr << "Failed to create instance: " << e.what() << std::endl;
        return false;
    }

    m_validationEnabled = YALN_ENABLE_VALIDATION_LAYER;
    setupDebugMessenger();
    return true;
}

void YalnVulkanInstance::setupDebugMessenger()
{
    if constexpr (!YALN_ENABLE_VALIDATION_LAYER)
    {
        return;
    }

    const auto createInfo = populateDebugMessengerCreateInfo();
    m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
}

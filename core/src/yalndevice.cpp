#include "../include/yalndevice.h"
#include "../include/yalnsurface.h"
#include "../include/yalnvulkaninstance.h"

#include <iostream>
#include <set>
#include <string>

bool YalnDevice::initialize(const IYalnInstance& instance, const IYalnSurface& surface)
{
    m_instance = &instance;
    m_surface  = &surface;

    if (!selectPhysicalDevice())
    {
        std::cerr << "Failed to find a suitable GPU" << std::endl;
        return false;
    }

    if (!createLogicalDevice())
    {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }

    return true;
}

void YalnDevice::waitIdle() const /*override*/
{
    if (m_device != nullptr)
    {
        m_device.waitIdle();
    }
}

bool YalnDevice::selectPhysicalDevice()
{
    auto devices = m_instance->handle().enumeratePhysicalDevices();
    if (devices.empty())
    {
        std::cerr << "Failed to find GPUs with Vulkan support!" << std::endl;
        return false;
    }

    for (auto& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            // vk::raii::PhysicalDevice 不可复制，只能移动
            m_physicalDevice = std::move(device);
            break;
        }
    }

    if (m_physicalDevice == nullptr)
    {
        std::cerr << "Failed to find a suitable GPU!" << std::endl;
        return false;
    }

    const auto props = m_physicalDevice.getProperties();
    std::cout << "Selected GPU: " << props.deviceName << std::endl;
    return true;
}

bool YalnDevice::isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice) const
{
    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    const bool extensionsSupported    = checkDeviceExtensionSupport(physicalDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        const SwapChainSupportDetails support = querySwapChainSupport(physicalDevice);
        swapChainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool YalnDevice::checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& physicalDevice) const
{
    const auto availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());
    for (const auto& ext : availableExtensions)
    {
        requiredExtensions.erase(std::string(ext.extensionName));
    }

    return requiredExtensions.empty();
}

YalnQueueFamilyIndices YalnDevice::findQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice) const
{
    QueueFamilyIndices indices;
    const auto queueFamilies = physicalDevice.getQueueFamilyProperties();

    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); i++)
    {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        // 部分驱动下 raii 句柄走 C API 更稳妥，这里用原始句柄
        if (m_surface != nullptr && m_surface->rawHandle() != VK_NULL_HANDLE)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(*physicalDevice, i, m_surface->rawHandle(), &presentSupport);
        }

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }
    }

    return indices;
}

YalnSwapChainSupportDetails YalnDevice::querySwapChainSupport(const vk::raii::PhysicalDevice& physicalDevice) const
{
    SwapChainSupportDetails details;

    if (m_surface == nullptr || !m_surface->isValid())
    {
        return details;
    }

    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(*m_surface->handle());
    details.formats      = physicalDevice.getSurfaceFormatsKHR(*m_surface->handle());
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(*m_surface->handle());

    return details;
}

bool YalnDevice::createLogicalDevice()
{
    m_queueIndices = findQueueFamilies(m_physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    const std::set<uint32_t> uniqueQueueFamilies = {
        m_queueIndices.graphicsFamily.value(),
        m_queueIndices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        queueCreateInfos.push_back({ {}, queueFamily, 1, &queuePriority });
    }

    vk::PhysicalDeviceFeatures deviceFeatures{};
    const auto& layers = YalnVulkanInstance::validationLayers();

    vk::DeviceCreateInfo createInfo(
        {},
        static_cast<uint32_t>(queueCreateInfos.size()),
        queueCreateInfos.data(),
        YALN_ENABLE_VALIDATION_LAYER ? static_cast<uint32_t>(layers.size()) : 0u,
        layers.data(),
        static_cast<uint32_t>(m_deviceExtensions.size()),
        m_deviceExtensions.data(),
        &deviceFeatures);

    try
    {
        m_device = vk::raii::Device(m_physicalDevice, createInfo);
    }
    catch (vk::SystemError& e)
    {
        std::cerr << "Failed to create logical device: " << e.what() << std::endl;
        return false;
    }

    m_graphicsQueue = vk::raii::Queue(m_device, m_queueIndices.graphicsFamily.value(), 0);
    m_presentQueue  = vk::raii::Queue(m_device, m_queueIndices.presentFamily.value(), 0);
    return true;
}

uint32_t YalnDevice::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
    const auto memProperties = m_physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void YalnDevice::printCapacity() const
{
    if (m_physicalDevice == nullptr)
    {
        std::cout << "No physical device selected." << std::endl;
        return;
    }

    const auto props = m_physicalDevice.getProperties();
    std::cout << "=== Physical Device Info ===" << std::endl;
    std::cout << "Device Name: " << props.deviceName << std::endl;
    std::cout << "API Version: "
              << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "."
              << VK_VERSION_PATCH(props.apiVersion) << std::endl;
    std::cout << "Driver Version: " << props.driverVersion << std::endl;
    std::cout << "Vendor ID: " << props.vendorID << std::endl;
    std::cout << "Device ID: " << props.deviceID << std::endl;

    const auto memProps = m_physicalDevice.getMemoryProperties();
    std::cout << "\nMemory Heaps: " << memProps.memoryHeapCount << std::endl;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++)
    {
        std::cout << "  Heap " << i << ": "
                  << (memProps.memoryHeaps[i].size / (1024 * 1024)) << " MB" << std::endl;
    }
}

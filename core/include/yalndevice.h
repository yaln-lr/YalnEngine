#ifndef YALN_DEVICE_H
#define YALN_DEVICE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <optional>
#include <vector>

#include "global_type.h"
#include "yalnvulkaninstance.h"

class YalnSurface;

// ============================================================================
//  YalnDevice
//  职责：物理设备挑选、队列族查找、逻辑设备与队列、显存类型查找、能力打印。
//        原 YalnView 的 Instance/Device 两段逻辑中被剥离出来的设备部分。
// ============================================================================
class YALN_CORE_EXPORT YalnDevice
{
public:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR        capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR>   presentModes;
    };

    YalnDevice() = default;
    ~YalnDevice() = default;

    YalnDevice(const YalnDevice&)            = delete;
    YalnDevice& operator=(const YalnDevice&) = delete;

    bool initialize(const YalnVulkanInstance& instance, const YalnSurface& surface);

    const vk::raii::PhysicalDevice& physicalDevice() const { return m_physicalDevice; }
    const vk::raii::Device&         device()         const { return m_device; }
    const vk::raii::Queue&          graphicsQueue()  const { return m_graphicsQueue; }
    const vk::raii::Queue&          presentQueue()   const { return m_presentQueue; }
    const QueueFamilyIndices&       queueFamilies()  const { return m_queueIndices; }

    bool isValid() const { return m_device != nullptr; }

    // 设备空闲（重建交换链 / 重建网格缓冲前调用）
    void waitIdle() const;

    QueueFamilyIndices      findQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice) const;
    SwapChainSupportDetails querySwapChainSupport(const vk::raii::PhysicalDevice& physicalDevice) const;
    uint32_t                findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

    void printCapacity() const;

private:
    bool selectPhysicalDevice();
    bool isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice) const;
    bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& physicalDevice) const;
    bool createLogicalDevice();

    const YalnVulkanInstance* m_instance = nullptr;
    const YalnSurface*        m_surface  = nullptr;

    vk::raii::PhysicalDevice m_physicalDevice{ nullptr };
    vk::raii::Device         m_device{ nullptr };
    vk::raii::Queue          m_graphicsQueue{ nullptr };
    vk::raii::Queue          m_presentQueue{ nullptr };
    QueueFamilyIndices       m_queueIndices;

    std::vector<const char*> m_deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};

#endif // YALN_DEVICE_H

#ifndef YALN_DEVICE_H
#define YALN_DEVICE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <optional>
#include <vector>

#include "global_type.h"
#include "yalninterfaces.h"

class YalnSurface;

// ============================================================================
//  YalnDevice —— IYalnDevice 默认实现
//  职责：物理设备挑选、队列族查找、逻辑设备与队列、显存类型查找、能力打印。
// ============================================================================
class YALN_CORE_EXPORT YalnDevice : public IYalnDevice
{
public:
    // 兼容别名：原内嵌结构已提升为接口层共享类型
    using QueueFamilyIndices       = YalnQueueFamilyIndices;
    using SwapChainSupportDetails  = YalnSwapChainSupportDetails;

    YalnDevice() = default;
    ~YalnDevice() override = default;

    YalnDevice(const YalnDevice&)            = delete;
    YalnDevice& operator=(const YalnDevice&) = delete;

    bool initialize(const IYalnInstance& instance, const IYalnSurface& surface) override;

    const vk::raii::PhysicalDevice& physicalDevice() const override { return m_physicalDevice; }
    const vk::raii::Device&         device()         const override { return m_device; }
    const vk::raii::Queue&          graphicsQueue()  const override { return m_graphicsQueue; }
    const vk::raii::Queue&          presentQueue()   const override { return m_presentQueue; }
    const YalnQueueFamilyIndices&   queueFamilies()  const override { return m_queueIndices; }

    bool isValid() const override { return m_device != nullptr; }

    // 设备空闲（重建交换链 / 重建网格缓冲前调用）
    void waitIdle() const override;

    QueueFamilyIndices      findQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice) const;
    SwapChainSupportDetails querySwapChainSupport(const vk::raii::PhysicalDevice& physicalDevice) const override;
    uint32_t                findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const override;

    void printCapacity() const;

private:
    bool selectPhysicalDevice();
    bool isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice) const;
    bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& physicalDevice) const;
    bool createLogicalDevice();

    const IYalnInstance* m_instance = nullptr;
    const IYalnSurface*  m_surface  = nullptr;

    vk::raii::PhysicalDevice m_physicalDevice{ nullptr };
    vk::raii::Device         m_device{ nullptr };
    vk::raii::Queue          m_graphicsQueue{ nullptr };
    vk::raii::Queue          m_presentQueue{ nullptr };
    QueueFamilyIndices       m_queueIndices;

    std::vector<const char*> m_deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};

#endif // YALN_DEVICE_H

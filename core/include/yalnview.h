#ifndef YALN_VIEW_H
#define YALN_VIEW_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <optional>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include "global_type.h"

#include "yalncamera.h"
#include "YalnMesh.h"
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class YALN_CORE_EXPORT YalnView
{
public:
    YalnView();
    virtual ~YalnView();
    bool initVulkan(const char* appname);

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR>  formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    static VKAPI_ATTR uint32_t VKAPI_CALL debugMessageFunc(
        vk::DebugUtilsMessageSeverityFlagBitsEXT       /*messageSeverity*/,
        vk::DebugUtilsMessageTypeFlagsEXT              /*messageTypes*/,
        vk::DebugUtilsMessengerCallbackDataEXT const * pCallbackData,
        void * /*pUserData*/
    )
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return false;
    }

    static constexpr vk::DebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo() {
        constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        );
        constexpr vk::DebugUtilsMessageTypeFlagsEXT    messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        );
        return { {}, severityFlags, messageTypeFlags, &debugMessageFunc };
    }

    // 输出硬件能力信息的函数
    void printCapacity();
    void setCameta(YalnCameraPtr camera)
    {
        m_camera_ptr = camera;
    };

protected:
    // 由子类实现：创建窗口表面
    virtual bool createSurface() = 0;

    // 由子类重写以处理调试消息
    virtual bool debugMessage(vk::DebugUtilsMessageSeverityFlagBitsEXT,
                              vk::DebugUtilsMessageTypeFlagsEXT,
                              vk::DebugUtilsMessengerCallbackDataEXT const*) { return true; }

    // 获取需要的实例级扩展（子类可重写以添加平台特定扩展）
    virtual std::vector<const char*> getRequiredInstanceExtensions();
    virtual void recreateSwapChain();
    // 初始化流程
    bool createInstance(const char* applicationName);
    bool checkValidationLayerSupport() const;
    void setupDebugMessenger();
    bool selectPhysicalDevice();
    bool isDeviceSuitable(vk::raii::PhysicalDevice& device);
    bool checkDeviceExtensionSupport(vk::raii::PhysicalDevice& device);
    QueueFamilyIndices findQueueFamilies(vk::raii::PhysicalDevice& device);
    SwapChainSupportDetails querySwapChainSupport(vk::raii::PhysicalDevice& device);
    bool createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();  // 保留兼容接口，内部调用setMesh
    void createIndexBuffer();    // 保留兼容接口，内部调用setMesh
    
    // 延迟加载：设置网格后自动创建/更新缓冲区
    void setMesh(YalnMesh* mesh);
    bool isMeshReady() const { return m_vertexBuffer != nullptr && m_indexBuffer != nullptr; }
    void createMeshBuffers();  // 延迟创建缓冲区
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::raii::Buffer& buffer, vk::raii::DeviceMemory& memory);
    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    void drawFrame();
    void cleanup();

    // 交换链辅助
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

protected:
    vk::raii::Context m_context;
    vk::raii::Instance m_instance{ nullptr };
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger{ nullptr };
    vk::raii::SurfaceKHR m_surface{ nullptr };
    vk::raii::PhysicalDevice m_physicalDevice{ nullptr };
    vk::raii::Device m_device{ nullptr };
    vk::raii::Queue m_graphicsQueue{ nullptr };
    vk::raii::Queue m_presentQueue{ nullptr };

    VkSurfaceKHR m_surfaceHandle = VK_NULL_HANDLE; // 原始句柄，用于C API查询

    vk::raii::SwapchainKHR m_swapChain{ nullptr };
    std::vector<vk::Image> m_swapChainImages;
    vk::Format m_swapChainImageFormat{};
    vk::Extent2D m_swapChainExtent{};
    std::vector<vk::raii::ImageView> m_swapChainImageViews;

    vk::raii::RenderPass m_renderPass{ nullptr };
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{ nullptr };
    vk::raii::PipelineLayout m_pipelineLayout{ nullptr };
    vk::raii::Pipeline m_graphicsPipeline{ nullptr };
    std::vector<vk::raii::Framebuffer> m_swapChainFramebuffers;

    vk::raii::CommandPool m_commandPool{ nullptr };

    vk::raii::Buffer m_vertexBuffer{ nullptr };
    vk::raii::DeviceMemory m_vertexBufferMemory{ nullptr };
    vk::raii::Buffer m_indexBuffer{ nullptr };
    vk::raii::DeviceMemory m_indexBufferMemory{ nullptr };
    std::vector<vk::raii::Buffer> m_uniformBuffers;
    std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;

    vk::raii::DescriptorPool m_descriptorPool{ nullptr };
    std::vector<vk::raii::DescriptorSet> m_descriptorSets;

    std::vector<vk::raii::CommandBuffer> m_commandBuffers;

    std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::raii::Fence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;

    uint32_t m_currentFrame = 0;
    bool m_framebufferResized = false;
    YalnCameraPtr m_camera_ptr = nullptr;
    
    // 延迟加载的网格数据
    YalnMesh* m_mesh = nullptr;
    uint32_t m_indexCount = 0;

private:
    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    
};

#endif //YALN_VIEW_H

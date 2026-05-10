#include "../include/yalnview.h"
#include "../include/yalnvertex.h"
#include <iostream>
#include <limits>
#include <algorithm>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
//#include "../hpp/vertexattribute.hpp"
#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYER = false;
#else
constexpr bool ENABLE_VALIDATION_LAYER = true;
#endif

// Uniform Buffer Object 结构体
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    // alignas(16) glm::mat4 view;
    // alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 proj_view_mat;
};

// ======================== 构造/析构 ========================

YalnView::YalnView()
{

}

YalnView::~YalnView()
{
    cleanup();
}

bool YalnView::initVulkan(const char* appname)
{
 // 2. 创建 Vulkan 实例
    if (!createInstance(appname))
    {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }

    // 3. 设置调试消息信使
    setupDebugMessenger();

    // 4. 创建窗口表面
    if (!createSurface())
    {
        std::cerr << "Failed to create surface" << std::endl;
        return false;
    }

    // 5. 选择物理设备
    if (!selectPhysicalDevice())
    {
        std::cerr << "Failed to find a suitable GPU" << std::endl;
        return false;
    }

    // 6. 创建逻辑设备
    if (!createLogicalDevice())
    {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }

    // 7. 创建交换链
    createSwapChain();

    // 8. 创建图像视图
    createImageViews();

    // 9. 创建渲染通道
    createRenderPass();

    // 10. 创建描述符集布局
    createDescriptorSetLayout();

    // 11. 创建图形管线
    createGraphicsPipeline();

    // 12. 创建帧缓冲区
    createFramebuffers();

    // 13. 创建命令池
    createCommandPool();

    // 14. 延迟创建顶点、索引缓冲区（由setMesh触发）

    // 15. 创建统一缓冲区
    createUniformBuffers();

    // 16. 创建描述符池和描述符集
    createDescriptorPool();
    createDescriptorSets();

    // 17. 创建命令缓冲区
    createCommandBuffers();

    // 18. 创建同步对象
    createSyncObjects();

    return true;
}
// ======================== Instance ========================

std::vector<const char*> YalnView::getRequiredInstanceExtensions()
{
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME
    };

#ifdef _WIN32
    requiredExtensions.push_back("VK_KHR_win32_surface");
#elif defined(__linux__)
    requiredExtensions.push_back("VK_KHR_xcb_surface");
#elif defined(__APPLE__)
    requiredExtensions.push_back("VK_MVK_macos_surface");
#endif

    if (ENABLE_VALIDATION_LAYER) {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return requiredExtensions;
}

void YalnView::recreateSwapChain()
{
    // 等待设备空闲
    m_device.waitIdle();

    // 清理现有资源
    m_swapChainFramebuffers.clear();
    m_swapChainImageViews.clear();
    m_swapChain = nullptr;

    // 重建交换链相关资源（使用动态视口，不需要重建管线）
    createSwapChain();
    createImageViews();
    createFramebuffers();
    m_framebufferResized = false;
}

bool YalnView::createInstance(const char* applicationName)
{
    if (ENABLE_VALIDATION_LAYER && !checkValidationLayerSupport()) {
        std::cerr << "Validation layer requested, but not available!" << std::endl;
        return false;
    }

    auto extensions = getRequiredInstanceExtensions();

    vk::ApplicationInfo appInfo(
        applicationName, 1, "YalnEngine", 1, VK_API_VERSION_1_0
    );

    vk::InstanceCreateInfo createInfo(
        {},
        &appInfo,
        ENABLE_VALIDATION_LAYER ? static_cast<uint32_t>(m_validationLayers.size()) : 0,
        m_validationLayers.data(),
        static_cast<uint32_t>(extensions.size()),
        extensions.data()
    );

    auto debugCreateInfo = populateDebugMessengerCreateInfo();
    if (ENABLE_VALIDATION_LAYER) {
        createInfo.pNext = &debugCreateInfo;
    }

    try {
        m_instance = vk::raii::Instance(m_context, createInfo);
    } catch (vk::SystemError& e) {
        std::cerr << "Failed to create instance: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool YalnView::checkValidationLayerSupport() const
{
    const auto layers = m_context.enumerateInstanceLayerProperties();
    std::set<std::string> requiredLayers(m_validationLayers.begin(), m_validationLayers.end());
    for (const auto& layer : layers) {
        requiredLayers.erase(layer.layerName);
    }
    return requiredLayers.empty();
}

void YalnView::setupDebugMessenger()
{
    if constexpr (!ENABLE_VALIDATION_LAYER) return;
    constexpr auto createInfo = populateDebugMessengerCreateInfo();
    m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
}

// ======================== Physical Device ========================

bool YalnView::selectPhysicalDevice()
{
    auto devices = m_instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        std::cerr << "Failed to find GPUs with Vulkan support!" << std::endl;
        return false;
    }

    for (auto& device : devices) {
        if (isDeviceSuitable(device)) {
            // vk::raii::PhysicalDevice 不能被复制，需要移动
            m_physicalDevice = std::move(device);
            break;
        }
    }

    if (*m_physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "Failed to find a suitable GPU!" << std::endl;
        return false;
    }

    auto props = m_physicalDevice.getProperties();
    std::cout << "Selected GPU: " << props.deviceName << std::endl;
    return true;
}

bool YalnView::isDeviceSuitable(vk::raii::PhysicalDevice& device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool YalnView::checkDeviceExtensionSupport(vk::raii::PhysicalDevice& device)
{
    auto availableExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());
    for (const auto& ext : availableExtensions) {
        requiredExtensions.erase(std::string(ext.extensionName));
    }
    return requiredExtensions.empty();
}

YalnView::QueueFamilyIndices YalnView::findQueueFamilies(vk::raii::PhysicalDevice& device)
{
    QueueFamilyIndices indices;
    auto queueFamilies = device.getQueueFamilyProperties();

    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); i++) {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        // 使用原始句柄查询
        if (m_surfaceHandle != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(*device, i, m_surfaceHandle, &presentSupport);
        }
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
    }
    return indices;
}

YalnView::SwapChainSupportDetails YalnView::querySwapChainSupport(vk::raii::PhysicalDevice& device)
{
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(*m_surface);

    details.formats = device.getSurfaceFormatsKHR(*m_surface);
    details.presentModes = device.getSurfacePresentModesKHR(*m_surface);

    return details;
}

// ======================== Logical Device ========================

bool YalnView::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        queueCreateInfos.push_back({
            {}, queueFamily, 1, &queuePriority
        });
    }

    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::DeviceCreateInfo createInfo(
        {},
        static_cast<uint32_t>(queueCreateInfos.size()),
        queueCreateInfos.data(),
        ENABLE_VALIDATION_LAYER ? static_cast<uint32_t>(m_validationLayers.size()) : 0,
        m_validationLayers.data(),
        static_cast<uint32_t>(m_deviceExtensions.size()),
        m_deviceExtensions.data(),
        &deviceFeatures
    );

    try {
        m_device = vk::raii::Device(m_physicalDevice, createInfo);
    } catch (vk::SystemError& e) {
        std::cerr << "Failed to create logical device: " << e.what() << std::endl;
        return false;
    }

    m_graphicsQueue = vk::raii::Queue(m_device, indices.graphicsFamily.value(), 0);
    m_presentQueue = vk::raii::Queue(m_device, indices.presentFamily.value(), 0);
    return true;
}

// ======================== Swap Chain ========================

vk::SurfaceFormatKHR YalnView::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR YalnView::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D YalnView::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    return capabilities.minImageExtent;
}

void YalnView::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    std::array<uint32_t, 2> queueFamilyIndices = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    vk::SwapchainCreateInfoKHR createInfo(
        {},
        *m_surface,
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive,
        0, nullptr,
        swapChainSupport.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        presentMode,
        VK_TRUE,
        nullptr
    );

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    }

    m_swapChain = vk::raii::SwapchainKHR(m_device, createInfo);
    m_swapChainImages = m_swapChain.getImages();
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

void YalnView::createImageViews()
{
    m_swapChainImageViews.clear();
    for (const auto& image : m_swapChainImages) {
        vk::ImageViewCreateInfo createInfo(
            {},
            image,
            vk::ImageViewType::e2D,
            m_swapChainImageFormat,
            {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        );
        m_swapChainImageViews.emplace_back(m_device, createInfo);
    }
}

// ======================== Render Pass ========================

void YalnView::createRenderPass()
{
    vk::AttachmentDescription colorAttachment(
        {},
        m_swapChainImageFormat,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR
    );

    vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass(
        {},
        vk::PipelineBindPoint::eGraphics,
        0, nullptr,
        1, &colorAttachmentRef
    );

    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        vk::AccessFlagBits::eColorAttachmentWrite
    );

    vk::RenderPassCreateInfo renderPassInfo(
        {},
        1, &colorAttachment,
        1, &subpass,
        1, &dependency
    );

    m_renderPass = vk::raii::RenderPass(m_device, renderPassInfo);
}

// ======================== Descriptor Set Layout ========================

void YalnView::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding(
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr
    );

    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &uboLayoutBinding);
    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_device, layoutInfo);
}

// ======================== Graphics Pipeline ========================

std::vector<char> YalnView::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

vk::raii::ShaderModule YalnView::createShaderModule(const std::vector<char>& code)
{
    // 确保代码大小是4的倍数（SPIR-V要求）
    vk::ShaderModuleCreateInfo createInfo(
        {},
        static_cast<uint32_t>(code.size()),
        reinterpret_cast<const uint32_t*>(code.data())
    );
    return vk::raii::ShaderModule(m_device, createInfo);
}

void YalnView::createGraphicsPipeline()
{
    std::cerr << "[DEBUG] createGraphicsPipeline called" << std::endl;
    auto vertShaderCode = readFile("../shaders/shader.vert.spv");
    std::cerr << "[DEBUG] Vertex shader size: " << vertShaderCode.size() << std::endl;
    auto fragShaderCode = readFile("../shaders/shader.frag.spv");
    std::cerr << "[DEBUG] Fragment shader size: " << fragShaderCode.size() << std::endl;

    auto vertShaderModule = createShaderModule(vertShaderCode);
    auto fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
        {},
        vk::ShaderStageFlagBits::eVertex,
        *vertShaderModule,
        "main"
    );
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
        {},
        vk::ShaderStageFlagBits::eFragment,
        *fragShaderModule,
        "main"
    );

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vertShaderStageInfo, fragShaderStageInfo
    };

    // 顶点输入 - 使用 Vk 版本的描述（YalnVertex 返回的是 C 类型）
    auto bindingDescription = YalnVertexPC::getBindingDescription();
    auto attributeDescriptions = YalnVertexPC::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
        {},
        1, reinterpret_cast<const vk::VertexInputBindingDescription*>(&bindingDescription),
        static_cast<uint32_t>(attributeDescriptions.size()),
        reinterpret_cast<const vk::VertexInputAttributeDescription*>(attributeDescriptions.data())
    );

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        {},
        vk::PrimitiveTopology::eTriangleList,
        VK_FALSE
    );

    // 使用动态视口和裁剪区域，这样 resize 时不需要重建管线
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo rasterizer(
        {},
        VK_FALSE,
        VK_FALSE,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise,
        VK_FALSE,
        0.0f, 0.0f, 0.0f,
        1.0f
    );

    vk::PipelineMultisampleStateCreateInfo multisampling(
        {},
        vk::SampleCountFlagBits::e1,
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE
    );

    vk::PipelineColorBlendAttachmentState colorBlendAttachment(
        VK_FALSE,
        vk::BlendFactor::eZero, vk::BlendFactor::eOne,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eZero, vk::BlendFactor::eOne,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    );

    vk::PipelineColorBlendStateCreateInfo colorBlending(
        {},
        VK_FALSE,
        vk::LogicOp::eCopy,
        1, &colorBlendAttachment,
        {0.0f, 0.0f, 0.0f, 0.0f}
    );

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
        {},
        1, &*m_descriptorSetLayout,
        0, nullptr
    );

    m_pipelineLayout = vk::raii::PipelineLayout(m_device, pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {},
        shaderStages,
        &vertexInputInfo,
        &inputAssembly,
        nullptr,
        &viewportState,
        &rasterizer,
        &multisampling,
        nullptr,
        &colorBlending,
        &dynamicStateInfo,  // 添加动态状态信息
        *m_pipelineLayout,
        *m_renderPass,
        0,
        nullptr,
        -1
    );

    // vk::raii::Pipeline 需要使用 std::vector<vk::raii::Pipeline> 创建
    // 单个管线需要通过 makePipeline 使用
    m_graphicsPipeline = vk::raii::Pipeline(m_device, nullptr, pipelineInfo);
}

// ======================== Framebuffers ========================

void YalnView::createFramebuffers()
{
    m_swapChainFramebuffers.clear();
    for (const auto& imageView : m_swapChainImageViews) {
        std::array<vk::ImageView, 1> attachments = {*imageView};

        vk::FramebufferCreateInfo framebufferInfo(
            {},
            *m_renderPass,
            static_cast<uint32_t>(attachments.size()),
            attachments.data(),
            m_swapChainExtent.width,
            m_swapChainExtent.height,
            1
        );
        m_swapChainFramebuffers.emplace_back(m_device, framebufferInfo);
    }
}

// ======================== Command Pool ========================

void YalnView::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

    vk::CommandPoolCreateInfo poolInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        queueFamilyIndices.graphicsFamily.value()
    );
    m_commandPool = vk::raii::CommandPool(m_device, poolInfo);
}

// ======================== Buffer Helpers ========================

uint32_t YalnView::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProperties = m_physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void YalnView::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                            vk::MemoryPropertyFlags properties,
                            vk::raii::Buffer& buffer, vk::raii::DeviceMemory& memory)
{
    vk::BufferCreateInfo bufferInfo({}, size, usage, vk::SharingMode::eExclusive);
    buffer = vk::raii::Buffer(m_device, bufferInfo);

    auto memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo(
        memRequirements.size,
        findMemoryType(memRequirements.memoryTypeBits, properties)
    );
    memory = vk::raii::DeviceMemory(m_device, allocInfo);
    buffer.bindMemory(*memory, 0);
}

void YalnView::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo allocInfo(
        *m_commandPool,
        vk::CommandBufferLevel::ePrimary,
        1
    );
    auto commandBuffer = std::move(m_device.allocateCommandBuffers(allocInfo)[0]);

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);

    vk::BufferCopy copyRegion(0, 0, size);
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

    commandBuffer.end();

    vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &*commandBuffer);
    m_graphicsQueue.submit(submitInfo);
    m_graphicsQueue.waitIdle();
}

// ======================== Mesh Setup (Lazy Loading) ========================

void YalnView::setMesh(YalnMesh* mesh)
{
    m_mesh = mesh;
    if (*m_device != VK_NULL_HANDLE && mesh && mesh->isValid()) {
        createMeshBuffers();
    }
}

void YalnView::createMeshBuffers()
{
    if (!m_mesh || !m_mesh->isValid()) {
        std::cerr << "YalnView::createMeshBuffers: Invalid mesh data!" << std::endl;
        return;
    }

    // 等待设备空闲再重建缓冲区
    m_device.waitIdle();

    // 清空旧缓冲区
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;

    // 获取顶点数据
    const auto& vertices = m_mesh->getVertices();
    const auto& indices = m_mesh->getIndices();
    m_indexCount = static_cast<uint32_t>(indices.size());

    // 创建顶点缓冲区
    vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
    vk::raii::Buffer stagingVertexBuffer{ nullptr };
    vk::raii::DeviceMemory stagingVertexMemory{ nullptr };
    createBuffer(vertexBufferSize,
                 vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingVertexBuffer, stagingVertexMemory);

    void* vertexData = stagingVertexMemory.mapMemory(0, vertexBufferSize);
    std::memcpy(vertexData, vertices.data(), static_cast<size_t>(vertexBufferSize));
    stagingVertexMemory.unmapMemory();

    createBuffer(vertexBufferSize,
                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 m_vertexBuffer, m_vertexBufferMemory);
    copyBuffer(*stagingVertexBuffer, *m_vertexBuffer, vertexBufferSize);

    // 创建索引缓冲区
    vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
    vk::raii::Buffer stagingIndexBuffer{ nullptr };
    vk::raii::DeviceMemory stagingIndexMemory{ nullptr };
    createBuffer(indexBufferSize,
                 vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 stagingIndexBuffer, stagingIndexMemory);

    void* indexData = stagingIndexMemory.mapMemory(0, indexBufferSize);
    std::memcpy(indexData, indices.data(), static_cast<size_t>(indexBufferSize));
    stagingIndexMemory.unmapMemory();

    createBuffer(indexBufferSize,
                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 m_indexBuffer, m_indexBufferMemory);
    copyBuffer(*stagingIndexBuffer, *m_indexBuffer, indexBufferSize);

    std::cout << "YalnView: Mesh buffers created (vertices: " << vertices.size() 
              << ", indices: " << indices.size() << ")" << std::endl;
}

// ======================== Vertex Buffer ========================

void YalnView::createVertexBuffer()
{
    // 使用YalnCube的默认实例
    YalnCube defaultCube;
    setMesh(&defaultCube);
}

// ======================== Index Buffer ========================

void YalnView::createIndexBuffer()
{
    // 索引缓冲区由setMesh统一创建
}

// ======================== Uniform Buffers ========================

void YalnView::createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(glm::mat4) * 3; // model + view + proj

    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();

    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        vk::raii::Buffer uniformBuffer{ nullptr };
        vk::raii::DeviceMemory uniformBufferMemory{ nullptr };
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     uniformBuffer, uniformBufferMemory);
        m_uniformBuffers.push_back(std::move(uniformBuffer));
        m_uniformBuffersMemory.push_back(std::move(uniformBufferMemory));
    }
}

// ======================== Descriptor Pool & Sets ========================

void YalnView::createDescriptorPool()
{
    std::array<vk::DescriptorPoolSize, 1> poolSizes = {{
        {vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(m_swapChainImages.size())}
    }};

    vk::DescriptorPoolCreateInfo poolInfo(
        {},
        static_cast<uint32_t>(m_swapChainImages.size()),
        static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data()
    );

    m_descriptorPool = vk::raii::DescriptorPool(m_device, poolInfo);
}

void YalnView::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(m_swapChainImages.size(), *m_descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo(
        *m_descriptorPool,
        static_cast<uint32_t>(layouts.size()),
        layouts.data()
    );

    m_descriptorSets = m_device.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < m_descriptorSets.size(); i++) {
        vk::DescriptorBufferInfo bufferInfo(
            *m_uniformBuffers[i],
            0,
            sizeof(UniformBufferObject)
        );

        std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {
            vk::WriteDescriptorSet(
                *m_descriptorSets[i],
                0, 0, 1,
                vk::DescriptorType::eUniformBuffer,
                nullptr,
                &bufferInfo,
                nullptr
            )
        };

        m_device.updateDescriptorSets(descriptorWrites, {});
    }
}

// ======================== Command Buffers ========================

void YalnView::createCommandBuffers()
{
    m_commandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo(
        *m_commandPool,
        vk::CommandBufferLevel::ePrimary,
        static_cast<uint32_t>(m_swapChainFramebuffers.size())
    );

    m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);
}

// ======================== Sync Objects ========================

void YalnView::createSyncObjects()
{
    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();
    m_imagesInFlight.resize(m_swapChainImages.size(), VK_NULL_HANDLE);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_imageAvailableSemaphores.emplace_back(m_device, semaphoreInfo);
        m_renderFinishedSemaphores.emplace_back(m_device, semaphoreInfo);
        m_inFlightFences.emplace_back(m_device, fenceInfo);
    }
}

// ======================== Draw Frame ========================

void YalnView::drawFrame()
{
    static int frameCount = 0;
    frameCount++;
    if (frameCount <= 3) {
        std::cerr << "[DEBUG] drawFrame called, frame #" << frameCount << std::endl;
    }
    
    // 延迟加载：网格未准备好则跳过渲染
    if (!isMeshReady()) {
        return;
    }

    auto& fence = m_inFlightFences[m_currentFrame];
    // 等待当前帧完成
    auto result = m_device.waitForFences({*fence}, VK_TRUE, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess) {
        std::cerr << "waitForFences failed" << std::endl;
        return;
    }

    // 获取交换链图像
    auto [acquireResult, imageIndex] = m_swapChain.acquireNextImage(
        std::numeric_limits<uint64_t>::max(),
        *m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE
    );

    if (acquireResult == vk::Result::eErrorOutOfDateKHR || acquireResult == vk::Result::eSuboptimalKHR) {
        // 重建交换链（需要在子类中处理）
        m_framebufferResized = true;
        return;
    }

    // 更新 uniform buffer
    //static auto startTime = std::chrono::high_resolution_clock::now();
    //auto currentTime = std::chrono::high_resolution_clock::now();
    //float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    // 与 YalnEngineC 保持一致 - 绕 Z 轴旋转
    //ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    // ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    // float aspect = static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
    // ubo.proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
    // ubo.proj[1][1] *= -1; // Vulkan Y轴向下
    ubo.model = glm::identity<glm::mat4>();
    ubo.proj_view_mat = m_camera_ptr->getMatrix();
    if (frameCount <= 3) {
        std::cerr << "[DEBUG] UniformBufferObject size: " << sizeof(UniformBufferObject) << std::endl;
        std::cerr << "[DEBUG] mat4 size: " << sizeof(glm::mat4) << std::endl;
        std::cerr << "[DEBUG] ubo.model[0]: " << ubo.model[0][0] << ", " << ubo.model[0][1] << ", " << ubo.model[0][2] << ", " << ubo.model[0][3] << std::endl;
    }

    void* data = m_uniformBuffersMemory[imageIndex].mapMemory(0, sizeof(ubo));
    std::memcpy(data, &ubo, sizeof(ubo));
    m_uniformBuffersMemory[imageIndex].unmapMemory();

    // 重置命令缓冲区
    m_commandBuffers[imageIndex].reset();

    // 记录命令
    vk::CommandBufferBeginInfo beginInfo{};
    m_commandBuffers[imageIndex].begin(beginInfo);

    vk::ClearValue clearColor(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.1f, 0.25f, 1.0f}));
    vk::RenderPassBeginInfo renderPassInfo(
        *m_renderPass,
        *m_swapChainFramebuffers[imageIndex],
        {{0, 0}, m_swapChainExtent},
        1, &clearColor
    );

    m_commandBuffers[imageIndex].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    m_commandBuffers[imageIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);

    // 设置动态视口和裁剪区域（支持 resize）
    vk::Viewport viewport(0.0f, 0.0f,
                          static_cast<float>(m_swapChainExtent.width),
                          static_cast<float>(m_swapChainExtent.height),
                          0.0f, 1.0f);
    m_commandBuffers[imageIndex].setViewport(0, viewport);

    vk::Rect2D scissor({0, 0}, m_swapChainExtent);
    m_commandBuffers[imageIndex].setScissor(0, scissor);

    m_commandBuffers[imageIndex].bindVertexBuffers(0, {*m_vertexBuffer}, {0});

    m_commandBuffers[imageIndex].bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint32);

    m_commandBuffers[imageIndex].bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *m_pipelineLayout,
        0, *m_descriptorSets[imageIndex],
        {}
    );

    // 绘制网格（索引数量由m_indexCount指定）
    m_commandBuffers[imageIndex].drawIndexed(m_indexCount, 1, 0, 0, 0);

    m_commandBuffers[imageIndex].endRenderPass();
    m_commandBuffers[imageIndex].end();

    // 重置 fence
    m_device.resetFences({*fence});

    // 提交命令
    std::array<vk::PipelineStageFlags, 1> waitStages = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };

    vk::SubmitInfo submitInfo(
        1, &*m_imageAvailableSemaphores[m_currentFrame],
        waitStages.data(),
        1, &*m_commandBuffers[imageIndex],
        1, &*m_renderFinishedSemaphores[m_currentFrame]
    );

    m_graphicsQueue.submit(submitInfo, *fence);

    // 呈现
    std::array<vk::SwapchainKHR, 1> swapChains = {*m_swapChain};
    vk::PresentInfoKHR presentInfo(
        1, &*m_renderFinishedSemaphores[m_currentFrame],
        static_cast<uint32_t>(swapChains.size()),
        swapChains.data(),
        &imageIndex
    );

    auto presentResult = m_presentQueue.presentKHR(presentInfo);

    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        m_framebufferResized = true;
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ======================== Cleanup ========================

void YalnView::cleanup()
{
    m_device.waitIdle();

    // 按创建的逆序销毁（RAII 会自动处理大部分，但我们需要确保正确的销毁顺序）
    // vk::raii 对象会按声明的逆序自动销毁，但成员变量按类声明的顺序析构
    // 这里不需要手动释放 RAII 资源
}

// ======================== Print Capacity ========================

void YalnView::printCapacity()
{
    if (*m_physicalDevice == VK_NULL_HANDLE) {
        std::cout << "No physical device selected." << std::endl;
        return;
    }

    auto props = m_physicalDevice.getProperties();
    std::cout << "=== Physical Device Info ===" << std::endl;
    std::cout << "Device Name: " << props.deviceName << std::endl;
    std::cout << "API Version: "
              << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "."
              << VK_VERSION_PATCH(props.apiVersion) << std::endl;
    std::cout << "Driver Version: " << props.driverVersion << std::endl;
    std::cout << "Vendor ID: " << props.vendorID << std::endl;
    std::cout << "Device ID: " << props.deviceID << std::endl;

    auto memProps = m_physicalDevice.getMemoryProperties();
    std::cout << "\nMemory Heaps: " << memProps.memoryHeapCount << std::endl;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        std::cout << "  Heap " << i << ": "
                  << (memProps.memoryHeaps[i].size / (1024 * 1024)) << " MB" << std::endl;
    }
}

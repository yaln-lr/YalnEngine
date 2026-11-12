#ifndef YALN_BUFFER_H
#define YALN_BUFFER_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>

#include "global_type.h"

class IYalnDevice;

// ============================================================================
//  YalnBuffer
//  职责：一块 Vulkan 缓冲 + 其绑定的显存，封装创建、写入、暂存拷贝。
// ============================================================================
class YALN_CORE_EXPORT YalnBuffer
{
public:
    YalnBuffer() = default;
    ~YalnBuffer() = default;

    YalnBuffer(const YalnBuffer&)            = delete;
    YalnBuffer& operator=(const YalnBuffer&) = delete;
    YalnBuffer(YalnBuffer&&) noexcept        = default;
    YalnBuffer& operator=(YalnBuffer&&) noexcept = default;

    // 分配缓冲并按需绑定显存
    bool create(const IYalnDevice&          device,
                vk::DeviceSize              size,
                vk::BufferUsageFlags        usage,
                vk::MemoryPropertyFlags     properties);

    void destroy();

    // 映射写入（要求内存为 hostVisible）
    void write(const void* data, vk::DeviceSize size, vk::DeviceSize offset = 0);

    bool         isValid() const { return m_buffer != nullptr; }
    vk::Buffer   handle()  const { return m_buffer; }
    vk::DeviceSize size()  const { return m_size; }

    // 用一次性命令把 src 拷到 dst（内部自行分配临时命令缓冲并等待完成）
    static bool copyBuffer(const IYalnDevice&               device,
                           const vk::raii::CommandPool&     commandPool,
                           vk::Buffer                       srcBuffer,
                           vk::Buffer                       dstBuffer,
                           vk::DeviceSize                   size);

    // 创建 + 上传数据 + 转移到设备本地显存（顶点/索引缓冲的常用路径）
    static bool uploadDeviceLocal(const IYalnDevice&           device,
                                  const vk::raii::CommandPool& commandPool,
                                  const void*                  data,
                                  vk::DeviceSize               size,
                                  vk::BufferUsageFlags         usage,
                                  YalnBuffer&                  outBuffer);

private:
    vk::raii::Buffer       m_buffer{ nullptr };
    vk::raii::DeviceMemory m_memory{ nullptr };
    vk::DeviceSize         m_size = 0;
};

#endif // YALN_BUFFER_H

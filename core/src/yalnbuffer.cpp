#include "../include/yalnbuffer.h"
#include "../include/yalninterfaces.h"

#include <cstring>
#include <stdexcept>

bool YalnBuffer::create(const IYalnDevice&       device,
                        vk::DeviceSize          size,
                        vk::BufferUsageFlags    usage,
                        vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo bufferInfo({}, size, usage, vk::SharingMode::eExclusive);
    m_buffer = vk::raii::Buffer(device.device(), bufferInfo);

    const auto memRequirements = m_buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo(
        memRequirements.size,
        device.findMemoryType(memRequirements.memoryTypeBits, properties));

    m_memory = vk::raii::DeviceMemory(device.device(), allocInfo);
    m_buffer.bindMemory(*m_memory, 0);
    m_size = size;
    return true;
}

void YalnBuffer::destroy()
{
    m_memory = nullptr;
    m_buffer = nullptr;
    m_size   = 0;
}

void YalnBuffer::write(const void* data, vk::DeviceSize size, vk::DeviceSize offset)
{
    if (!isValid() || data == nullptr || size == 0)
    {
        return;
    }

    void* mapped = m_memory.mapMemory(offset, size);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    m_memory.unmapMemory();
}

bool YalnBuffer::copyBuffer(const IYalnDevice&            device,
                            const vk::raii::CommandPool& commandPool,
                            vk::Buffer                   srcBuffer,
                            vk::Buffer                   dstBuffer,
                            vk::DeviceSize               size)
{
    vk::CommandBufferAllocateInfo allocInfo(
        *commandPool,
        vk::CommandBufferLevel::ePrimary,
        1);

    auto commandBuffer = std::move(device.device().allocateCommandBuffers(allocInfo)[0]);

    const vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);

    const vk::BufferCopy copyRegion(0, 0, size);
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

    commandBuffer.end();

    const vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &*commandBuffer);
    device.graphicsQueue().submit(submitInfo);
    device.graphicsQueue().waitIdle();

    return true;
}

bool YalnBuffer::uploadDeviceLocal(const IYalnDevice&            device,
                                   const vk::raii::CommandPool& commandPool,
                                   const void*                  data,
                                   vk::DeviceSize               size,
                                   vk::BufferUsageFlags         usage,
                                   YalnBuffer&                  outBuffer)
{
    if (data == nullptr || size == 0)
    {
        return false;
    }

    // 1) 暂存缓冲（CPU 可见）
    YalnBuffer staging;
    if (!staging.create(device, size,
                        vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        return false;
    }
    staging.write(data, size);

    // 2) 目标缓冲（设备本地）
    if (!outBuffer.create(device, size,
                          usage | vk::BufferUsageFlagBits::eTransferDst,
                          vk::MemoryPropertyFlagBits::eDeviceLocal))
    {
        return false;
    }

    // 3) 拷贝
    return copyBuffer(device, commandPool, staging.handle(), outBuffer.handle(), size);
}

#ifndef YALN_SCENE_H
#define YALN_SCENE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <vector>

#include "global_type.h"
#include "yalnbuffer.h"

class YalnMesh;
class YalnDevice;

// 单个网格在 GPU 上的资源
struct YALN_CORE_EXPORT YalnGpuMesh {
    YalnBuffer vertexBuffer;
    YalnBuffer indexBuffer;
    uint32_t   indexCount = 0;
};

// ============================================================================
//  YalnScene
//  职责：场景容器。持有 CPU 端网格引用与对应的 GPU 顶点/索引缓冲，
//        不参与任何命令录制或帧循环。
// ============================================================================
class YALN_CORE_EXPORT YalnScene
{
public:
    YalnScene() = default;

    YalnScene(const YalnScene&)            = delete;
    YalnScene& operator=(const YalnScene&) = delete;

    void addMesh(YalnMesh* mesh);
    void clear();

    bool isEmpty() const { return m_meshes.empty(); }
    // 每个网格都已生成 GPU 缓冲
    bool isReady() const { return !m_meshes.empty() && m_meshes.size() == m_gpuMeshes.size(); }

    // 重建全部网格的 GPU 缓冲（需调用方先让设备空闲）
    bool buildGpuBuffers(const YalnDevice& device, const vk::raii::CommandPool& commandPool);

    const std::vector<YalnMesh*>&   meshes()    const { return m_meshes; }
    const std::vector<YalnGpuMesh>& gpuMeshes() const { return m_gpuMeshes; }

private:
    std::vector<YalnMesh*>   m_meshes;
    std::vector<YalnGpuMesh> m_gpuMeshes;
};

#endif // YALN_SCENE_H

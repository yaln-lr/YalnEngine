#include "../include/yalnscene.h"
#include "../include/yalndevice.h"
#include "../include/yalnmesh.h"

#include <array>
#include <iostream>
#include <limits>
#include <set>

YalnScene::YalnScene()
{
    // 默认根节点：组节点（用户可 setRoot 整体替换）
    m_root = std::make_unique<YalnGroupNode>("Root");
}

// ======================== 场景树 ========================

void YalnScene::setRoot(std::unique_ptr<YalnNode> root)
{
    if (root == nullptr)
    {
        std::cerr << "YalnScene::setRoot: 根节点不能为空，已忽略" << std::endl;
        return;
    }
    m_root = std::move(root);
    m_meshToGpu.clear();
    m_drawItems.clear();
}

void YalnScene::update(const glm::vec3& cameraPosition)
{
    // 1. 自顶向下重算世界矩阵（相机位置供公告板等行为节点使用）
    m_root->updateWorld(glm::mat4(1.0f), cameraPosition);

    // 2. 收集可见绘制实例并绑定 GPU 缓冲
    m_drawItems.clear();

    std::vector<YalnDrawableInstance> instances;
    m_root->collectInstances(instances);

    m_drawItems.reserve(instances.size());
    for (const YalnDrawableInstance& instance : instances)
    {
        const auto it = m_meshToGpu.find(instance.drawable->mesh());
        if (it == m_meshToGpu.end())
        {
            // GPU 缓冲未构建（改树后忘了 rebuild）：跳过该项而不是崩溃
            continue;
        }

        YalnDrawItem item;
        item.gpuMesh  = &m_gpuMeshes[it->second];
        item.world    = instance.world;
        item.material = instance.drawable->material();
        item.passMask = instance.drawable->passMask();
        item.pipelineOverride = instance.drawable->pipelineOverride();
        item.mesh     = instance.drawable->mesh();

        glm::vec3 localMin(0.0f);
        glm::vec3 localMax(0.0f);
        if (item.mesh != nullptr && item.mesh->getLocalBounds(localMin, localMax))
        {
            const std::array<glm::vec3, 8> corners = {
                glm::vec3(localMin.x, localMin.y, localMin.z),
                glm::vec3(localMax.x, localMin.y, localMin.z),
                glm::vec3(localMin.x, localMax.y, localMin.z),
                glm::vec3(localMax.x, localMax.y, localMin.z),
                glm::vec3(localMin.x, localMin.y, localMax.z),
                glm::vec3(localMax.x, localMin.y, localMax.z),
                glm::vec3(localMin.x, localMax.y, localMax.z),
                glm::vec3(localMax.x, localMax.y, localMax.z)
            };

            item.boundsMin = glm::vec3(std::numeric_limits<float>::max());
            item.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

            for (const glm::vec3& corner : corners)
            {
                const glm::vec3 worldCorner =
                    glm::vec3(instance.world * glm::vec4(corner, 1.0f));
                item.boundsMin = glm::min(item.boundsMin, worldCorner);
                item.boundsMax = glm::max(item.boundsMax, worldCorner);
            }
        }

        m_drawItems.push_back(item);
    }
}

// ======================== 兼容扁平接口 ========================

void YalnScene::addMesh(YalnMesh* mesh)
{
    if (mesh == nullptr)
    {
        return;
    }
    m_root->attachDrawable(std::make_unique<YalnMeshDrawable>(mesh));
}

void YalnScene::clear()
{
    m_root = std::make_unique<YalnGroupNode>("Root");
    m_meshToGpu.clear();
    m_drawItems.clear();
    m_meshes.clear();
    m_gpuMeshes.clear();
    m_lights.clear();
}

// ======================== GPU 缓冲 ========================

void YalnScene::collectUniqueMeshes(const YalnNode& node, std::vector<YalnMesh*>& out,
                                    std::unordered_map<YalnMesh*, size_t>& seen) const
{
    for (const auto& drawable : node.drawables())
    {
        YalnMesh* mesh = drawable->mesh();
        if (mesh != nullptr && mesh->isValid() && seen.find(mesh) == seen.end())
        {
            seen[mesh] = out.size();
            out.push_back(mesh);
        }
    }
    for (const auto& child : node.children())
    {
        collectUniqueMeshes(*child, out, seen);
    }
}

bool YalnScene::buildGpuBuffers(const IYalnDevice& device, const vk::raii::CommandPool& commandPool)
{
    // 1. 遍历场景树收集唯一网格（同一网格多处引用只建一份缓冲）
    m_meshes.clear();
    m_meshToGpu.clear();
    std::unordered_map<YalnMesh*, size_t> seen;
    collectUniqueMeshes(*m_root, m_meshes, seen);

    if (m_meshes.empty())
    {
        std::cerr << "YalnScene::buildGpuBuffers: 场景树中没有可绘制的网格" << std::endl;
        return false;
    }

    m_gpuMeshes.clear();
    m_gpuMeshes.reserve(m_meshes.size());

    for (size_t i = 0; i < m_meshes.size(); ++i)
    {
        YalnMesh* mesh = m_meshes[i];
        const auto& indices  = mesh->getIndices();
        const void* vertexData = mesh->getVertexData();
        const size_t vertexCount = mesh->getVertexCount();
        const size_t vertexStride = mesh->getVertexStride();

        if (vertexData == nullptr || vertexCount == 0 || vertexStride == 0 || indices.empty())
        {
            continue;
        }

        YalnGpuMesh gpuMesh;
        gpuMesh.indexCount = static_cast<uint32_t>(indices.size());

        const vk::DeviceSize vertexSize = vertexStride * vertexCount;
        if (!YalnBuffer::uploadDeviceLocal(device, commandPool,
                                           vertexData, vertexSize,
                                           vk::BufferUsageFlagBits::eVertexBuffer,
                                           gpuMesh.vertexBuffer))
        {
            continue;
        }

        const vk::DeviceSize indexSize = sizeof(indices[0]) * indices.size();
        if (!YalnBuffer::uploadDeviceLocal(device, commandPool,
                                           indices.data(), indexSize,
                                           vk::BufferUsageFlagBits::eIndexBuffer,
                                           gpuMesh.indexBuffer))
        {
            continue;
        }

        m_meshToGpu[mesh] = m_gpuMeshes.size();
        m_gpuMeshes.push_back(std::move(gpuMesh));

        std::cout << "YalnScene: mesh buffer created [" << i << "] (vertices: "
                  << vertexCount << ", indices: " << indices.size() << ")" << std::endl;
    }

    return !m_gpuMeshes.empty();
}

#include "../include/yalnscene.h"
#include "../include/yalndevice.h"
#include "../include/yalnmesh.h"

#include <iostream>
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
        m_drawItems.push_back({ &m_gpuMeshes[it->second], instance.world });
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
        const auto& vertices = mesh->getVertices();
        const auto& indices  = mesh->getIndices();

        YalnGpuMesh gpuMesh;
        gpuMesh.indexCount = static_cast<uint32_t>(indices.size());

        const vk::DeviceSize vertexSize = sizeof(vertices[0]) * vertices.size();
        if (!YalnBuffer::uploadDeviceLocal(device, commandPool,
                                           vertices.data(), vertexSize,
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
                  << vertices.size() << ", indices: " << indices.size() << ")" << std::endl;
    }

    return !m_gpuMeshes.empty();
}

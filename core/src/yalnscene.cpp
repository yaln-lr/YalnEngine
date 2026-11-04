#include "../include/yalnscene.h"
#include "../include/yalndevice.h"
#include "../include/yalnmesh.h"

#include <iostream>

void YalnScene::addMesh(YalnMesh* mesh)
{
    m_meshes.push_back(mesh);
}

void YalnScene::clear()
{
    m_gpuMeshes.clear();
    m_meshes.clear();
}

bool YalnScene::buildGpuBuffers(const YalnDevice& device, const vk::raii::CommandPool& commandPool)
{
    if (m_meshes.empty())
    {
        std::cerr << "YalnScene::buildGpuBuffers: no meshes to build" << std::endl;
        return false;
    }

    m_gpuMeshes.clear();
    m_gpuMeshes.reserve(m_meshes.size());

    for (size_t i = 0; i < m_meshes.size(); ++i)
    {
        YalnMesh* mesh = m_meshes[i];
        if (mesh == nullptr || !mesh->isValid())
        {
            std::cerr << "YalnScene::buildGpuBuffers: invalid mesh at index " << i << std::endl;
            continue;
        }

        const auto& vertices = mesh->getVertices();
        const auto& indices  = mesh->getIndices();
        if (vertices.empty() || indices.empty())
        {
            std::cerr << "YalnScene::buildGpuBuffers: empty mesh at index " << i << std::endl;
            continue;
        }

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

        std::cout << "YalnScene: mesh buffer created [" << i << "] (vertices: "
                  << vertices.size() << ", indices: " << indices.size() << ")" << std::endl;

        m_gpuMeshes.push_back(std::move(gpuMesh));
    }

    return !m_gpuMeshes.empty();
}

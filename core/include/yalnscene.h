#ifndef YALN_SCENE_H
#define YALN_SCENE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

#include "global_type.h"
#include "yalninterfaces.h"   // YalnGpuMesh / YalnDrawItem / IYalnDevice / IYalnScene
#include "yalnnode.h"

// ============================================================================
//  YalnScene —— IYalnScene 默认实现（场景树）
//
//  持有一棵以 root 为根的节点树（根默认是组节点）：
//    - update()：每帧自顶向下重算世界矩阵，收集可见绘制项（drawItems）；
//    - buildGpuBuffers()：遍历树收集"唯一网格"去重后建 GPU 缓冲，
//      同一网格被多个节点/绘制项引用时只占一份显存；
//    - addMesh()：兼容扁平接口，等价于根节点下挂一个网格绘制项。
//
//  不参与命令录制与帧循环。
// ============================================================================
class YALN_CORE_EXPORT YalnScene : public IYalnScene
{
public:
    YalnScene();
    ~YalnScene() override = default;

    YalnScene(const YalnScene&)            = delete;
    YalnScene& operator=(const YalnScene&) = delete;

    // ---- 场景树 ----
    void setRoot(std::unique_ptr<YalnNode> root) override;
    YalnNode* root() override { return m_root.get(); }

    void update(const glm::vec3& cameraPosition = glm::vec3(0.0f)) override;
    const std::vector<YalnDrawItem>& drawItems() const override { return m_drawItems; }

    // ---- 兼容扁平接口 ----
    void addMesh(YalnMesh* mesh) override;
    void clear() override;

    // ---- 灯光（场景拥有所有权；clear() 一并清空）----
    void addLight(std::unique_ptr<YalnLight> light) override { m_lights.push_back(std::move(light)); }
    const std::vector<std::unique_ptr<YalnLight>>& lights() const override { return m_lights; }

    bool isEmpty() const override { return m_root->countDrawables() == 0; }
    bool isReady() const override { return !m_gpuMeshes.empty() && !isEmpty(); }

    // 遍历场景树收集唯一网格并重建全部 GPU 缓冲（需调用方先让设备空闲）
    bool buildGpuBuffers(const IYalnDevice& device, const vk::raii::CommandPool& commandPool) override;

    const std::vector<YalnMesh*>&   meshes()    const override { return m_meshes; }
    const std::vector<YalnGpuMesh>& gpuMeshes() const override { return m_gpuMeshes; }

private:
    // 深度优先收集子树中的唯一网格（按出现顺序去重）
    void collectUniqueMeshes(const YalnNode& node, std::vector<YalnMesh*>& out,
                             std::unordered_map<YalnMesh*, size_t>& seen) const;

    std::unique_ptr<YalnNode> m_root;   // 根节点（默认组节点，永远非空）

    std::vector<YalnMesh*>   m_meshes;     // 去重后的唯一网格（build 后有效）
    std::vector<YalnGpuMesh> m_gpuMeshes;
    std::unordered_map<YalnMesh*, size_t> m_meshToGpu;   // 网格 → GPU 缓冲下标

    std::vector<YalnDrawItem> m_drawItems;   // update() 后有效

    std::vector<std::unique_ptr<YalnLight>> m_lights;   // 场景级光源
};

#endif // YALN_SCENE_H

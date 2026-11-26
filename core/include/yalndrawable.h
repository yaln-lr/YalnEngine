#ifndef YALN_DRAWABLE_H
#define YALN_DRAWABLE_H

#include <glm/glm.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "global_type.h"
#include "yalnmath.h"
#include "yalnrendertypes.h"

class YalnMesh;

// ============================================================================
//  YalnDrawable —— 可绘制对象的抽象
//
//  挂在节点上、每帧产生一次 draw call 的最小单元。
//  不拥有网格（网格归上层/资源管理），只持有引用与节点内局部变换。
//  局部朝向使用 Eigen 四元数（无万向死锁）。
//
//  扩展点：派生可实现带材质参数、多子网格引用、实例化绘制等绘制项。
// ============================================================================
class YALN_CORE_EXPORT YalnDrawable
{
public:
    virtual ~YalnDrawable() = default;

    virtual YalnMesh* mesh() const = 0;

    // 节点内局部变换（最终世界矩阵 = 节点世界矩阵 × 本矩阵）
    virtual glm::mat4 localMatrix() const = 0;

    virtual bool visible() const = 0;
    virtual void setVisible(bool visible) = 0;

    // 材质与 Pass 选择（默认值保持旧行为：默认材质、参与全部 Pass）。
    // 派生绘制项可覆盖，实现按对象/按状态切换材质或 Pass。
    // 主材质：由 setMaterial 设置，Opaque Pass 使用它。
    virtual YalnMaterialHandle material() const { return YALN_DEFAULT_MATERIAL; }
    virtual void setMaterial([[maybe_unused]] YalnMaterialHandle material) {}

    // 额外叠加材质：每个 drawable 独立持有，由对应 passMask 的 Pass 叠加绘制。
    // 默认空实现，旧派生类无需修改。
    virtual const std::vector<YalnMaterialHandle>& overlayMaterials() const
    {
        static const std::vector<YalnMaterialHandle> empty;
        return empty;
    }
    virtual void addOverlayMaterial([[maybe_unused]] YalnMaterialHandle material) {}
    virtual bool removeOverlayMaterial([[maybe_unused]] YalnMaterialHandle material)
    {
        return false;
    }
    virtual void clearOverlayMaterials() {}

    virtual YalnPassMask passMask() const { return YALN_PASS_ALL; }
    virtual void setPassMask([[maybe_unused]] YalnPassMask passMask) {}

    // 可选：跳过材质中的管线选择，直接指定 PSO（默认无效 = 走材质）
    virtual YalnPipelineHandle pipelineOverride() const { return YALN_INVALID_PIPELINE; }
    virtual void setPipelineOverride([[maybe_unused]] YalnPipelineHandle pipeline) {}
};

// ============================================================================
//  YalnMeshDrawable —— 网格绘制项默认实现（T·R·S，朝向为四元数）
// ============================================================================
class YALN_CORE_EXPORT YalnMeshDrawable : public YalnDrawable
{
public:
    explicit YalnMeshDrawable(YalnMesh* mesh)
        : m_mesh(mesh)
    {
    }

    YalnMesh* mesh() const override { return m_mesh; }

    glm::mat4 localMatrix() const override
    {
        return toGlmTRS(m_position, m_orientation, m_scale);
    }

    bool visible() const override { return m_visible; }
    void setVisible(bool visible) override { m_visible = visible; }

    YalnMaterialHandle material() const override { return m_material; }
    void setMaterial(YalnMaterialHandle material) override { m_material = material; }

    const std::vector<YalnMaterialHandle>& overlayMaterials() const override
    {
        return m_overlayMaterials;
    }
    void addOverlayMaterial(YalnMaterialHandle material) override
    {
        m_overlayMaterials.push_back(material);
    }
    bool removeOverlayMaterial(YalnMaterialHandle material) override
    {
        const auto it = std::find(m_overlayMaterials.begin(), m_overlayMaterials.end(), material);
        if (it == m_overlayMaterials.end())
        {
            return false;
        }
        m_overlayMaterials.erase(it);
        return true;
    }
    void clearOverlayMaterials() override { m_overlayMaterials.clear(); }

    YalnPassMask passMask() const override { return m_passMask; }
    void setPassMask(YalnPassMask passMask) override { m_passMask = passMask; }

    YalnPipelineHandle pipelineOverride() const override { return m_pipelineOverride; }
    void setPipelineOverride(YalnPipelineHandle pipeline) override { m_pipelineOverride = pipeline; }

    // ---- 局部变换 ----
    void setPosition(const Eigen::Vector3f& p)        { m_position = p; }
    void setScale(const Eigen::Vector3f& s)           { m_scale = s; }
    void setOrientation(const Eigen::Quaternionf& q)  { m_orientation = q.normalized(); }

    // 欧拉角便捷入口（弧度，yaw/pitch/roll，本征序 ZYX）
    void setEulerRotation(const Eigen::Vector3f& eulerYPR)
    {
        m_orientation = eulerToQuat(eulerYPR);
    }

    // 增量旋转（局部空间，四元数复合）
    void rotate(const Eigen::Vector3f& axis, float angleRadians)
    {
        m_orientation = (m_orientation * axisAngleToQuat(axis, angleRadians)).normalized();
    }

    const Eigen::Vector3f&    position()    const { return m_position; }
    const Eigen::Vector3f&    scale()       const { return m_scale; }
    const Eigen::Quaternionf& orientation() const { return m_orientation; }

private:
    YalnMesh* m_mesh = nullptr;   // 非拥有引用
    bool      m_visible = true;

    YalnMaterialHandle m_material = YALN_DEFAULT_MATERIAL;
    std::vector<YalnMaterialHandle> m_overlayMaterials;
    YalnPassMask       m_passMask = YALN_PASS_ALL;
    YalnPipelineHandle m_pipelineOverride = YALN_INVALID_PIPELINE;

    Eigen::Vector3f       m_position{ Eigen::Vector3f::Zero() };
    Eigen::Quaternionf    m_orientation{ Eigen::Quaternionf::Identity() };
    Eigen::Vector3f       m_scale{ Eigen::Vector3f::Ones() };
};

// ============================================================================
//  遍历中间产物：节点树收集出的"绘制实例"（场景层再绑定 GPU 缓冲）
// ============================================================================
struct YALN_CORE_EXPORT YalnDrawableInstance {
    YalnDrawable* drawable = nullptr;
    glm::mat4     world{ 1.0f };
};

#endif // YALN_DRAWABLE_H

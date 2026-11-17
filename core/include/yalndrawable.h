#ifndef YALN_DRAWABLE_H
#define YALN_DRAWABLE_H

#include <glm/glm.hpp>

#include <string>

#include "global_type.h"
#include "yalnmath.h"

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

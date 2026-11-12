#ifndef YALN_DRAWABLE_H
#define YALN_DRAWABLE_H

#include <glm/glm.hpp>

#include <string>

#include "global_type.h"

class YalnMesh;

// ============================================================================
//  YalnDrawable —— 可绘制对象的抽象
//
//  挂在节点上、每帧产生一次 draw call 的最小单元。
//  不拥有网格（网格归上层/资源管理），只持有引用与节点内局部变换。
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
//  YalnMeshDrawable —— 网格绘制项默认实现（TRS 局部变换）
// ============================================================================
class YALN_CORE_EXPORT YalnMeshDrawable : public YalnDrawable
{
public:
    explicit YalnMeshDrawable(YalnMesh* mesh,
                              glm::vec3 position = { 0.0f, 0.0f, 0.0f },
                              glm::vec3 rotation = { 0.0f, 0.0f, 0.0f },
                              glm::vec3 scale    = { 1.0f, 1.0f, 1.0f })
        : m_mesh(mesh)
        , m_position(position)
        , m_rotation(rotation)
        , m_scale(scale)
    {
    }

    YalnMesh* mesh() const override { return m_mesh; }

    glm::mat4 localMatrix() const override
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, m_position);
        m = glm::rotate(m, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::rotate(m, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        return glm::scale(m, m_scale);
    }

    bool visible() const override { return m_visible; }
    void setVisible(bool visible) override { m_visible = visible; }

    // ---- 局部 TRS ----
    void setPosition(const glm::vec3& p) { m_position = p; }
    void setRotation(const glm::vec3& r) { m_rotation = r; }   // 弧度
    void setScale(const glm::vec3& s)    { m_scale = s; }

    const glm::vec3& position() const { return m_position; }
    const glm::vec3& rotation() const { return m_rotation; }
    const glm::vec3& scale()    const { return m_scale; }

private:
    YalnMesh* m_mesh = nullptr;   // 非拥有引用
    bool      m_visible = true;

    glm::vec3 m_position;
    glm::vec3 m_rotation;
    glm::vec3 m_scale;
};

// ============================================================================
//  遍历中间产物：节点树收集出的"绘制实例"（场景层再绑定 GPU 缓冲）
// ============================================================================
struct YALN_CORE_EXPORT YalnDrawableInstance {
    YalnDrawable* drawable = nullptr;
    glm::mat4     world{ 1.0f };
};

#endif // YALN_DRAWABLE_H

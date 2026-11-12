#ifndef YALN_CAMERA_H
#define YALN_CAMERA_H

#include "global_type.h"

#include <glm/glm.hpp>
#include <memory>

// ============================================================================
//  YalnCamera —— 相机抽象接口（纯虚，不含任何实现与状态）
//
//  变化点：不同场景需要不同相机（透视 / 正交 / 轨道环绕 / 第一人称 / 自由飞行）。
//  上层（YalnView / YalnRenderer / QtViewer）只依赖本接口，
//  新增相机 = 新增一个派生类，上层零改动。
// ============================================================================
class YALN_CORE_EXPORT YalnCamera
{
public:
    virtual ~YalnCamera() = default;

    // ---- 矩阵 ----
    virtual glm::mat4 getViewMatrix()       = 0;
    virtual glm::mat4 getProjectionMatrix() = 0;
    virtual glm::mat4 getMatrix()           = 0;  // proj * view

    // ---- 姿态（各相机自行解释 position/target 的含义）----
    virtual void      setPosition(const glm::vec3& position) = 0;
    virtual glm::vec3 getPosition() const                    = 0;
    virtual void      setTarget(const glm::vec3& target)     = 0;
    virtual glm::vec3 getTarget() const                      = 0;
    virtual void      setUp(const glm::vec3& up)             = 0;

    // ---- 投影参数（各派生类自行决定含义与默认值）----
    virtual void setPerspective(float fov, float aspect, float nearPlane, float farPlane) = 0;
    // 当窗口大小改变时更新宽高比
    virtual void setAspectRatio(float aspect) = 0;
};
using YalnCameraPtr = std::shared_ptr<YalnCamera>;

// ============================================================================
//  YalnCameraBase —— lookAt 型相机的公共实现基类
//
//  提供 position / target / up 共享状态、视图矩阵与 proj*view 的默认实现。
//  常规相机（透视、正交、轨道）继承它即可，只需补投影矩阵；
//  特殊相机（如自由飞行）也可直接实现 YalnCamera 接口。
// ============================================================================
class YALN_CORE_EXPORT YalnCameraBase : public YalnCamera
{
public:
    YalnCameraBase();
    virtual ~YalnCameraBase() = default;

    glm::mat4 getMatrix() override;      // getProjectionMatrix() * getViewMatrix()
    glm::mat4 getViewMatrix() override;  // glm::lookAt(position, target, up)

    void      setPosition(const glm::vec3& position) override { m_position = position; }
    glm::vec3 getPosition() const                    override { return m_position; }
    void      setTarget(const glm::vec3& target)     override { m_target = target; }
    glm::vec3 getTarget() const                      override { return m_target; }
    void      setUp(const glm::vec3& up)             override { m_up = up; }

    // 投影方式是相机子类的本质差异，保持纯虚强制子类给出实现
    void setPerspective(float fov, float aspect, float nearPlane, float farPlane) override = 0;
    void setAspectRatio(float aspect) override = 0;

protected:
    glm::vec3 m_position;  // 相机位置
    glm::vec3 m_target;    // 观察目标点
    glm::vec3 m_up;        // 向上方向
};

#endif // YALN_CAMERA_H

#ifndef YALN_CAMERA_FIRSTPERSON_H
#define YALN_CAMERA_FIRSTPERSON_H

#include "yalnperspectivecamera.h"
#include "yalnmath.h"

// ============================================================================
//  YalnFirstPersonCamera —— 第一人称自由飞行相机
//
//  朝向由四元数决定（Eigen，无万向死锁）：pitch 可连续转过 ±90° 天顶/
//  天底而不发生姿态翻转，不存在欧拉角方案的俯仰死区。配合
//  moveForward/moveRight/moveUp 实现飞行控制。
//
//  局部坐标系约定：forward = 朝向作用在 +X 上；yaw 绕世界 Z 轴（左乘），
//  pitch 绕本地 Y 轴（右乘）。抬头为正。
//  投影复用透视相机，仅替换朝向与视图矩阵。
// ============================================================================
class YALN_CORE_EXPORT YalnFirstPersonCamera : public YalnPerspectiveCamera
{
public:
    YalnFirstPersonCamera(
        float fov = 45.0f,
        float aspect = 16.0f / 9.0f,
        float nearPlane = 0.1f,
        float farPlane = 100.0f
    );
    virtual ~YalnFirstPersonCamera() = default;

    // ---- 朝向控制 ----
    // 一次设置 yaw/pitch（单位：弧度；兼容入口，内部转四元数）
    void setRotation(float yaw, float pitch);

    // 增量旋转：四元数复合。pitch 无 ±89° 限制——可连续越过天顶/天底，
    // 不再有欧拉角方案的万向死锁限幅
    void rotate(float yawDelta, float pitchDelta);

    // 朝向整体赋值（四元数，自动归一化）
    void setOrientation(const Eigen::Quaternionf& q) { m_orientation = q.normalized(); }
    const Eigen::Quaternionf& orientation() const    { return m_orientation; }

    // 从四元数反解欧拉分量（诊断/调试用）
    float getYaw() const;
    float getPitch() const;

    // ---- 移动控制 ----
    void moveForward(float dist);  // 沿视线方向（含垂直分量）
    void moveRight(float dist);    // 沿视线右方向（水平）
    void moveUp(float dist);       // 沿世界上方向 (0,0,1)

    // 视线方向 / 右方向（单位向量）
    glm::vec3 forward() const;
    glm::vec3 right() const;

    // ---- 接口适配：朝向由四元数决定，不使用目标点 ----
    glm::mat4 getViewMatrix() override;
    // 把"看向某点"转换为朝向四元数（可看向任意方向，包括天顶/天底）
    void setTarget(const glm::vec3& target) override;
    // 向上方向固定为世界 Z 轴，忽略外部设置
    void setUp([[maybe_unused]] const glm::vec3& up) override {}
    // 目标点等价于 position + forward
    glm::vec3 getTarget() const override { return m_position + forward(); }

private:
    Eigen::Quaternionf m_orientation{ Eigen::Quaternionf::Identity() };
};
using YalnFirstPersonCameraPtr = std::shared_ptr<YalnFirstPersonCamera>;
#endif // YALN_CAMERA_FIRSTPERSON_H

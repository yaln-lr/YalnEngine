#ifndef YALN_CAMERA_FIRSTPERSON_H
#define YALN_CAMERA_FIRSTPERSON_H

#include "yalnperspectivecamera.h"

// ============================================================================
//  YalnFirstPersonCamera —— 第一人称自由飞行相机
//
//  朝向由 yaw/pitch 决定（不依赖目标点），配合 moveForward/moveRight/moveUp
//  实现飞行控制。适合场景漫游 / 飞行视角 / FPS 类观察。
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
    // 一次设置 yaw/pitch（单位：弧度）
    void setRotation(float yaw, float pitch);
    // 增量旋转：pitch 自动限制在 ±89°，避免视线翻转
    void rotate(float yawDelta, float pitchDelta);

    float getYaw() const   { return m_yaw; }
    float getPitch() const { return m_pitch; }

    // ---- 移动控制 ----
    void moveForward(float dist);  // 沿视线方向（含垂直分量）
    void moveRight(float dist);    // 沿视线右方向（水平）
    void moveUp(float dist);       // 沿世界上方向 (0,0,1)

    // 视线方向 / 右方向（单位向量）
    glm::vec3 forward() const;
    glm::vec3 right() const;

    // ---- 接口适配：朝向由 yaw/pitch 决定，不使用目标点 ----
    glm::mat4 getViewMatrix() override;
    // 把"看向某点"转换为 yaw/pitch
    void setTarget(const glm::vec3& target) override;
    // 向上方向固定为世界 Z 轴，忽略外部设置
    void setUp(const glm::vec3& up) override { (void)up; }
    // 目标点等价于 position + forward
    glm::vec3 getTarget() const override { return m_position + forward(); }

private:
    float m_yaw   = 0.0f;  // 方位角（绕 Z 轴，弧度）
    float m_pitch = 0.0f;  // 俯仰角（抬头为正）
};
using YalnFirstPersonCameraPtr = std::shared_ptr<YalnFirstPersonCamera>;
#endif // YALN_CAMERA_FIRSTPERSON_H

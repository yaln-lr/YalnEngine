#ifndef YALN_CAMERA_ORBIT_H
#define YALN_CAMERA_ORBIT_H

#include "yalnperspectivecamera.h"

// ============================================================================
//  YalnOrbitCamera —— 轨道环绕相机
//
//  始终看向目标点，用球坐标（方位角 yaw / 仰角 pitch / 距离 distance）定位，
//  位置由这三参数推导。适合模型查看器 / 场景编辑器的环绕观察。
//  投影复用透视相机，仅替换定位方式。
// ============================================================================
class YALN_CORE_EXPORT YalnOrbitCamera : public YalnPerspectiveCamera
{
public:
    YalnOrbitCamera(
        float fov = 45.0f,
        float aspect = 16.0f / 9.0f,
        float nearPlane = 0.1f,
        float farPlane = 100.0f
    );
    virtual ~YalnOrbitCamera() = default;

    // 一次设置全部轨道参数（yaw/pitch 单位：弧度）
    void setOrbit(float yaw, float pitch, float distance);

    // 环绕旋转：鼠标拖拽时调用（pitch 自动限制在 ±89°，避免翻转）
    void rotate(float yawDelta, float pitchDelta);

    // 缩放：正值拉近，负值拉远（距离最小 0.01，避免穿入目标点）
    void zoom(float distanceDelta) { setDistance(m_distance + distanceDelta); }
    void setDistance(float distance);

    float getYaw() const      { return m_yaw; }
    float getPitch() const    { return m_pitch; }
    float getDistance() const { return m_distance; }

    // ---- 接口适配：目标/位置变化时同步球坐标 ----
    // 移动目标点：保持 yaw/pitch/distance 不变，跟随移动
    void setTarget(const glm::vec3& target) override;
    // 直接指定位置：反解出 yaw/pitch/distance（这样 QtViewer 的
    // "旋转位置后调 setPosition" 的自动旋转逻辑无需修改即可生效）
    void setPosition(const glm::vec3& position) override;

private:
    // 由当前 yaw/pitch/distance/target 重算相机位置
    void updatePosition();

    float m_yaw      = glm::radians(45.0f);   // 方位角（绕 Z 轴）
    float m_pitch    = glm::radians(30.0f);   // 仰角（Z 轴向上为正）
    float m_distance = 5.0f;                  // 到目标点距离
};
using YalnOrbitCameraPtr = std::shared_ptr<YalnOrbitCamera>;
#endif // YALN_CAMERA_ORBIT_H

#include "../include/yalnfirstpersoncamera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace {
constexpr glm::vec3 kWorldUp(0.0f, 0.0f, 1.0f);   // 本引擎使用 Z 轴向上
}

YalnFirstPersonCamera::YalnFirstPersonCamera(float fov, float aspect, float nearPlane, float farPlane)
    : YalnPerspectiveCamera(fov, aspect, nearPlane, farPlane)
{
}

// ==================== 朝向（四元数） ====================

void YalnFirstPersonCamera::setRotation(float yaw, float pitch)
{
    // 兼容入口：欧拉 → 四元数。yaw 绕世界 Z（左乘），pitch 绕本地 Y（右乘），
    // 抬头为正（绕 +Y 的 -pitch 使 +X 转向 +Z）。
    m_orientation =
        Eigen::Quaternionf(Eigen::AngleAxisf(yaw,   Eigen::Vector3f::UnitZ())) *
        Eigen::Quaternionf(Eigen::AngleAxisf(-pitch, Eigen::Vector3f::UnitY()));
    m_orientation.normalize();
}

void YalnFirstPersonCamera::rotate(float yawDelta, float pitchDelta)
{
    // 偏航：绕世界竖直轴左乘（与欧拉方案语义一致）
    if (yawDelta != 0.0f)
    {
        m_orientation =
            (Eigen::Quaternionf(Eigen::AngleAxisf(yawDelta, Eigen::Vector3f::UnitZ())) *
             m_orientation).normalized();
    }
    // 俯仰：绕本地 Y 轴右乘——四元数复合，pitch 可连续越过 ±90°，无万向死锁
    if (pitchDelta != 0.0f)
    {
        m_orientation =
            (m_orientation *
             Eigen::Quaternionf(Eigen::AngleAxisf(-pitchDelta, Eigen::Vector3f::UnitY())))
                .normalized();
    }
}

float YalnFirstPersonCamera::getYaw() const
{
    const glm::vec3 f = forward();
    return std::atan2(f.y, f.x);
}

float YalnFirstPersonCamera::getPitch() const
{
    return std::asin(glm::clamp(forward().z, -1.0f, 1.0f));
}

// ==================== 方向 ====================

glm::vec3 YalnFirstPersonCamera::forward() const
{
    // 朝向作用在本地 +X 上（约定：本地 +X = 视线方向）
    return toGlm(m_orientation * Eigen::Vector3f::UnitX());
}

glm::vec3 YalnFirstPersonCamera::right() const
{
    glm::vec3 r = glm::cross(forward(), kWorldUp);
    const float len = glm::length(r);
    if (len < 1e-5f) {
        // 视线几乎垂直（看向正上/正下方）：用本地 Y 轴的世界投影兜底
        return toGlm(m_orientation * Eigen::Vector3f::UnitY());
    }
    return r / len;
}

// ==================== 移动 ====================

void YalnFirstPersonCamera::moveForward(float dist)
{
    m_position += forward() * dist;
}

void YalnFirstPersonCamera::moveRight(float dist)
{
    m_position += right() * dist;
}

void YalnFirstPersonCamera::moveUp(float dist)
{
    m_position += kWorldUp * dist;
}

// ==================== 视图 ====================

glm::mat4 YalnFirstPersonCamera::getViewMatrix()
{
    const glm::vec3 f = forward();

    // lookAt 的 up 与视线几乎平行时退化：改用本地 Y 轴的世界投影作为参考上方向
    glm::vec3 up = kWorldUp;
    if (std::abs(glm::dot(f, up)) > 0.999f)
    {
        up = toGlm(m_orientation * Eigen::Vector3f::UnitY());
    }

    return glm::lookAt(m_position, m_position + f, up);
}

void YalnFirstPersonCamera::setTarget(const glm::vec3& target)
{
    const glm::vec3 dir = target - m_position;
    const float len = glm::length(dir);
    if (len < 1e-5f) return;

    // 反解 yaw/pitch 后走兼容构造。无 ±89° 限幅：可看向天顶/天底
    const float pitch = std::asin(glm::clamp(dir.z / len, -1.0f, 1.0f));
    const float yaw   = std::atan2(dir.y, dir.x);
    setRotation(yaw, pitch);
}

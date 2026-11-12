#include "../include/yalnorbitcamera.h"

#include <glm/glm.hpp>
#include <cmath>

namespace {
constexpr float kMaxPitch = glm::radians(89.0f);
}

YalnOrbitCamera::YalnOrbitCamera(float fov, float aspect, float nearPlane, float farPlane)
    : YalnPerspectiveCamera(fov, aspect, nearPlane, farPlane)
{
    updatePosition();
}

void YalnOrbitCamera::setOrbit(float yaw, float pitch, float distance)
{
    m_yaw      = yaw;
    m_pitch    = glm::clamp(pitch, -kMaxPitch, kMaxPitch);
    m_distance = glm::max(distance, 0.01f);
    updatePosition();
}

void YalnOrbitCamera::rotate(float yawDelta, float pitchDelta)
{
    m_yaw   += yawDelta;
    m_pitch  = glm::clamp(m_pitch + pitchDelta, -kMaxPitch, kMaxPitch);
    updatePosition();
}

void YalnOrbitCamera::setDistance(float distance)
{
    m_distance = glm::max(distance, 0.01f);
    updatePosition();
}

void YalnOrbitCamera::setTarget(const glm::vec3& target)
{
    m_target = target;
    updatePosition();
}

void YalnOrbitCamera::setPosition(const glm::vec3& position)
{
    const glm::vec3 offset = position - m_target;
    const float dist = glm::length(offset);

    if (dist > 1e-5f) {
        m_distance = dist;
        // Z 轴向上的球坐标反解（与 setOrbit/rotate 保持同样的限幅）
        m_pitch = glm::clamp(offset.z / dist, -1.0f, 1.0f);
        m_pitch = glm::clamp(std::asin(m_pitch), -kMaxPitch, kMaxPitch);
        m_yaw   = std::atan2(offset.y, offset.x);
    }
    m_position = position;
}

void YalnOrbitCamera::updatePosition()
{
    m_position = m_target + m_distance * glm::vec3(
        std::cos(m_yaw) * std::cos(m_pitch),
        std::sin(m_yaw) * std::cos(m_pitch),
        std::sin(m_pitch));
}

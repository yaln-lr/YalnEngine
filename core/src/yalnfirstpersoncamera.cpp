#include "../include/yalnfirstpersoncamera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace {
constexpr glm::vec3 kWorldUp(0.0f, 0.0f, 1.0f);   // 本引擎使用 Z 轴向上
constexpr float     kMaxPitch = glm::radians(89.0f);
}

YalnFirstPersonCamera::YalnFirstPersonCamera(float fov, float aspect, float nearPlane, float farPlane)
    : YalnPerspectiveCamera(fov, aspect, nearPlane, farPlane)
{
}

void YalnFirstPersonCamera::setRotation(float yaw, float pitch)
{
    m_yaw   = yaw;
    m_pitch = glm::clamp(pitch, -kMaxPitch, kMaxPitch);
}

void YalnFirstPersonCamera::rotate(float yawDelta, float pitchDelta)
{
    m_yaw   += yawDelta;
    m_pitch  = glm::clamp(m_pitch + pitchDelta, -kMaxPitch, kMaxPitch);
}

glm::vec3 YalnFirstPersonCamera::forward() const
{
    return glm::vec3(
        std::cos(m_yaw) * std::cos(m_pitch),
        std::sin(m_yaw) * std::cos(m_pitch),
        std::sin(m_pitch));
}

glm::vec3 YalnFirstPersonCamera::right() const
{
    glm::vec3 r = glm::cross(forward(), kWorldUp);
    const float len = glm::length(r);
    if (len < 1e-5f) {
        // 视线几乎垂直（看向正上/正下方），退化为水平右方向
        return glm::vec3(-std::sin(m_yaw), std::cos(m_yaw), 0.0f);
    }
    return r / len;
}

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

glm::mat4 YalnFirstPersonCamera::getViewMatrix()
{
    return glm::lookAt(m_position, m_position + forward(), kWorldUp);
}

void YalnFirstPersonCamera::setTarget(const glm::vec3& target)
{
    const glm::vec3 dir = target - m_position;
    const float len = glm::length(dir);
    if (len < 1e-5f) return;

    m_pitch = glm::clamp(dir.z / len, -1.0f, 1.0f);
    m_pitch = glm::clamp(std::asin(m_pitch), -kMaxPitch, kMaxPitch);
    m_yaw   = std::atan2(dir.y, dir.x);
}

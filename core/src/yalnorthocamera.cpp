#include "../include/yalnorthocamera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

YalnOrthoCamera::YalnOrthoCamera(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    : m_left(left)
    , m_right(right)
    , m_bottom(bottom)
    , m_top(top)
    , m_near(nearPlane)
    , m_far(farPlane)
{
}

void YalnOrthoCamera::setPerspective(float fov, float aspect, float nearPlane, float farPlane)
{
    // 以 near 平面处的可视高度为正交高度，保持与同参数透视相机画面范围一致
    const float halfHeight = nearPlane * std::tan(glm::radians(fov) * 0.5f);
    m_top    = halfHeight;
    m_bottom = -halfHeight;
    m_right  = halfHeight * aspect;
    m_left   = -m_right;
    m_near   = nearPlane;
    m_far    = farPlane;
}

void YalnOrthoCamera::setAspectRatio(float aspect)
{
    if (aspect <= 0.0f) return;
    const float halfHeight = (m_top - m_bottom) * 0.5f;
    m_right = halfHeight * aspect;
    m_left  = -m_right;
}

glm::mat4 YalnOrthoCamera::getProjectionMatrix()
{
    glm::mat4 mat = glm::ortho(m_left, m_right, m_bottom, m_top, m_near, m_far);
    mat[1][1] *= -1; // Vulkan Y轴向下
    return mat;
}

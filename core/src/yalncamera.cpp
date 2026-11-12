#include "../include/yalncamera.h"

#include <glm/gtc/matrix_transform.hpp>

// 默认姿态：位于 (2,2,2) 看向原点，Z 轴向上（与场景几何约定一致）
YalnCameraBase::YalnCameraBase()
    : m_position(2.0f, 2.0f, 2.0f)
    , m_target(0.0f, 0.0f, 0.0f)
    , m_up(0.0f, 0.0f, 1.0f)
{
}

glm::mat4 YalnCameraBase::getMatrix()
{
    return getProjectionMatrix() * getViewMatrix();
}

glm::mat4 YalnCameraBase::getViewMatrix()
{
    return glm::lookAt(m_position, m_target, m_up);
}

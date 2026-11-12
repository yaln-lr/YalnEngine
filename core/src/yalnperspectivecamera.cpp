#include "../include/yalnperspectivecamera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


YalnPerspectiveCamera::YalnPerspectiveCamera(float fov,float aspect ,float nearPlane,float farPlane)
    : m_fov(fov)
    , m_aspect(aspect)
    , m_near(nearPlane)
    , m_far(farPlane)
{
}

glm::mat4 YalnPerspectiveCamera::getProjectionMatrix()
{
    // 透视投影；Vulkan 的 NDC Y 轴向下，需要翻转
    glm::mat4 mat = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
    mat[1][1] *= -1; // Vulkan Y轴向下
    return mat;
}

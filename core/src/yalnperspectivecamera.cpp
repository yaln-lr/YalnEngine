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
// 设置透视投影参数
glm::mat4 YalnPerspectiveCamera::getViewMatrix()
{
    // ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 YalnPerspectiveCamera::getProjectionMatrix()
{
        
    // float aspect = static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
    // ubo.proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
    // ubo.proj[1][1] *= -1; // Vulkan Y轴向下
    glm::mat4 mat = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
    mat[1][1] *= -1; // Vulkan Y轴向下
    return mat;
}


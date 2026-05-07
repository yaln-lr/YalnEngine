#ifndef YALN_CAMERA_PERSPECTIVE_H
#define YALN_CAMERA_PERSPECTIVE_H

#include "yalncamera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class YALN_CORE_EXPORT YalnPerspectiveCamera : public YalnCamera
{
public:
    YalnPerspectiveCamera(
        float fov = 45.0f,
        float aspect = 16.0f / 9.0f,
        float nearPlane = 0.1f,
        float farPlane = 100.0f
    );
    virtual ~YalnPerspectiveCamera() = default;

    // 设置透视投影参数
    void setPerspective(float fov, float aspect, float nearPlane, float farPlane) override
    {
        m_fov = fov;
        m_aspect = aspect;
        m_near = nearPlane;
        m_far = farPlane;
    }

    // 当窗口大小改变时更新宽高比
    void setAspectRatio(float aspect) override { m_aspect = aspect; }

protected:
    virtual glm::mat4 getViewMatrix() override;

    virtual glm::mat4 getProjectionMatrix() override;

private:
    float m_fov;           // 视野角度（度数）
    float m_aspect;        // 宽高比
    float m_near;          // 近裁剪面
    float m_far;           // 远裁剪面
};
using YalnPerspectiveCameraPtr = std::shared_ptr<YalnPerspectiveCamera>;
#endif // YALN_CAMERA_PERSPECTIVE_H
#ifndef YALN_CAMERA_PERSPECTIVE_H
#define YALN_CAMERA_PERSPECTIVE_H

#include "yalncamera.h"
#include <glm/glm.hpp>

// ============================================================================
//  YalnPerspectiveCamera —— 透视投影相机
//  继承 YalnCameraBase（视图矩阵 / 姿态已有默认实现），只补投影部分。
// ============================================================================
class YALN_CORE_EXPORT YalnPerspectiveCamera : public YalnCameraBase
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

    // 覆写保持 public：接口 YalnCamera 中声明为 public，
    // 收窄到 protected 会破坏接口契约（具体类型无法直接调用）
    virtual glm::mat4 getProjectionMatrix() override;

private:
    float m_fov;           // 视野角度（度数）
    float m_aspect;        // 宽高比
    float m_near;          // 近裁剪面
    float m_far;           // 远裁剪面
};
using YalnPerspectiveCameraPtr = std::shared_ptr<YalnPerspectiveCamera>;
#endif // YALN_CAMERA_PERSPECTIVE_H

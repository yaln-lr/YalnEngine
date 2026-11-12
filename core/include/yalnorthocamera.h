#ifndef YALN_CAMERA_ORTHO_H
#define YALN_CAMERA_ORTHO_H

#include "yalncamera.h"
#include <glm/glm.hpp>

// ============================================================================
//  YalnOrthoCamera —— 正交投影相机
//
//  物体大小不随距离变化，适合 2D 视图 / CAD / 等轴测 / UI 背景。
//  视图与姿态复用 YalnCameraBase（lookAt），仅替换投影方式。
// ============================================================================
class YALN_CORE_EXPORT YalnOrthoCamera : public YalnCameraBase
{
public:
    YalnOrthoCamera(
        float left = -1.0f,
        float right = 1.0f,
        float bottom = -1.0f,
        float top = 1.0f,
        float nearPlane = 0.1f,
        float farPlane = 100.0f
    );
    virtual ~YalnOrthoCamera() = default;

    // 直接设置正交视口范围
    void setOrtho(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        m_left = left;
        m_right = right;
        m_bottom = bottom;
        m_top = top;
        m_near = nearPlane;
        m_far = farPlane;
    }

    // 接口兼容：将 fov/aspect 映射为正交视口
    // （以 near 平面处的可视高度 = near * tan(fov/2) 为准，方便与透视相机切换）
    void setPerspective(float fov, float aspect, float nearPlane, float farPlane) override;

    // 保持垂直半高不变，按宽高比调整水平范围（窗口缩放时调用）
    void setAspectRatio(float aspect) override;

    float getLeft() const   { return m_left; }
    float getRight() const  { return m_right; }
    float getBottom() const { return m_bottom; }
    float getTop() const    { return m_top; }

    // 覆写保持 public：与接口 YalnCamera 的访问级别一致
    virtual glm::mat4 getProjectionMatrix() override;

private:
    float m_left;
    float m_right;
    float m_bottom;
    float m_top;
    float m_near;   // 近裁剪面
    float m_far;    // 远裁剪面
};
using YalnOrthoCameraPtr = std::shared_ptr<YalnOrthoCamera>;
#endif // YALN_CAMERA_ORTHO_H

#ifndef YALN_CAMERA_H
#define YALN_CAMERA_H

#include "global_type.h"

#include <glm/glm.hpp>
#include <memory>

class YALN_CORE_EXPORT YalnCamera
{
public:
    YalnCamera()
        : m_position(2.0f, 2.0f, 2.0f)
        , m_target(0.0f, 0.0f, 0.0f)
        , m_up(0.0f, 0.0f, 1.0f)
    {};
    virtual ~YalnCamera(){};

    virtual glm::mat4 getMatrix();

    void setPosition(const glm::vec3& position) { m_position = position; }
    glm::vec3 getPosition() const { return m_position; }

    // 设置相机看向的目标点
    void setTarget(const glm::vec3& target) { m_target = target; }
    glm::vec3 getTarget() const { return m_target; }

    // 设置向上方向
    void setUp(const glm::vec3& up) { m_up = up; }

    // 设置透视投影参数
    virtual void setPerspective(float /*fov*/, float /*aspect*/, float /*nearPlane*/, float /*farPlane*/){};
    // 当窗口大小改变时更新宽高比
    virtual void setAspectRatio(float /*aspect*/) {};

    virtual glm::mat4 getViewMatrix() = 0;

    virtual glm::mat4 getProjectionMatrix() = 0;

protected:
    glm::vec3 m_position;  // 相机位置
    glm::vec3 m_target;    // 观察目标点
    glm::vec3 m_up;        // 向上方向
};
using YalnCameraPtr = std::shared_ptr<YalnCamera>;
#endif // YALN_CAMERA_H
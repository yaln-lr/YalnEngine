#ifndef YALN_MATH_H
#define YALN_MATH_H

// ============================================================================
//  数学桥接层
//
//  旋转表示统一为 Eigen 四元数（消除万向死锁）：
//    - 节点朝向、绘制项朝向、第一人称相机朝向、自旋动画，内部全部是四元数；
//    - 增量旋转用四元数复合（AngleAxis × quat），任意俯仰无翻转死区；
//    - 矩阵输出边界（localMatrix / 视图矩阵）转换为 glm::mat4——
//      Vulkan 上传（UBO / Push Constants / 顶点属性）的内存布局不变，
//      渲染层与着色器零感知。
//
//  欧拉角约定（仅用于便捷入口 setEulerRotation，内部立即转为四元数）：
//    Z 轴向上引擎：yaw(Z) → pitch(Y) → roll(X)，本征序 ZYX。
// ============================================================================

#include <Eigen/Geometry>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ---- 朝向统一类型 ----
using YalnQuat = Eigen::Quaternionf;

// ---- Eigen ↔ glm 桥接 ----

// Eigen 四元数 (x,y,z,w 存储) → glm 四元数 (w,x,y,z)
inline glm::quat toGlm(const Eigen::Quaternionf& q)
{
    return glm::quat(q.w(), q.x(), q.y(), q.z());
}

inline Eigen::Quaternionf toEigen(const glm::quat& q)
{
    return Eigen::Quaternionf(q.w, q.x, q.y, q.z);
}

inline glm::vec3 toGlm(const Eigen::Vector3f& v)
{
    return glm::vec3(v.x(), v.y(), v.z());
}

inline Eigen::Vector3f toEigen(const glm::vec3& v)
{
    return Eigen::Vector3f(v.x, v.y, v.z);
}

inline glm::mat4 toGlm(const Eigen::Matrix4f& m)
{
    // Eigen 默认列主序，与 glm::mat4 内存布局一致，逐列拷贝
    glm::mat4 out(1.0f);
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            out[c][r] = m(r, c);
        }
    }
    return out;
}

// 四元数 → glm 旋转矩阵（转出为列主序 mat4，与 Vulkan 上传布局一致）
inline glm::mat4 toGlmRotation(const Eigen::Quaternionf& q)
{
    return glm::mat4_cast(toGlm(q));
}

// TRS 组装（Eigen 内部计算，输出 glm 边界矩阵）
inline glm::mat4 toGlmTRS(const Eigen::Vector3f&    position,
                          const Eigen::Quaternionf& orientation,
                          const Eigen::Vector3f&    scale)
{
    Eigen::Affine3f t = Eigen::Affine3f::Identity();
    t.translate(position);          // T
    t.rotate(orientation);          // R（四元数，无万向死锁）
    t.scale(scale);                 // S
    return toGlm(t.matrix());
}

// ---- 便捷构造 ----

// 欧拉角（弧度，yaw/pitch/roll 分量）→ 四元数（本征序 ZYX：先 Z 后 Y 后 X）
inline Eigen::Quaternionf eulerToQuat(const Eigen::Vector3f& eulerYPR)
{
    Eigen::Quaternionf q =
        Eigen::AngleAxisf(eulerYPR.z(), Eigen::Vector3f::UnitZ()) *
        Eigen::AngleAxisf(eulerYPR.y(), Eigen::Vector3f::UnitY()) *
        Eigen::AngleAxisf(eulerYPR.x(), Eigen::Vector3f::UnitX());
    return q.normalized();
}

// 轴角 → 四元数（零轴兜底为单位 Z）
inline Eigen::Quaternionf axisAngleToQuat(const Eigen::Vector3f& axis, float angleRadians)
{
    const float len = axis.norm();
    const Eigen::Vector3f a =
        len > 1e-6f ? Eigen::Vector3f(axis / len) : Eigen::Vector3f::UnitZ();  // 物化避免表达式模板类型不匹配
    return Eigen::Quaternionf(Eigen::AngleAxisf(angleRadians, a));
}

#endif // YALN_MATH_H

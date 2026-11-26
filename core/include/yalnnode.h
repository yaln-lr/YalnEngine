#ifndef YALN_NODE_H
#define YALN_NODE_H

#include <glm/glm.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "global_type.h"
#include "yalndrawable.h"
#include "yalnmath.h"

// ============================================================================
//  场景树节点体系（旋转统一使用 Eigen 四元数，无万向死锁）
//
//  结构节点：
//    YalnNode       普通节点：可以包含子节点，也可以包含可绘制对象
//    YalnGroupNode  组节点：只能包含子节点（纯组织用途，挂绘制项被拒绝）
//    YalnLeafNode   叶子节点：只能包含可绘制对象（加子节点被拒绝）
//    YalnSwitchNode 开关节点：多个子节点只激活一个（多状态/多方案切换）
//
//  变换节点族（语义单一，方便动画与装配）：
//    YalnTranslateNode 纯位移    YalnRotateNode 纯旋转（四元数）
//    YalnScaleNode     纯缩放    YalnSpinNode   绕轴持续自旋（四元数累乘）
//
//  行为节点：
//    YalnBillboardNode 圆柱公告板：绕 Z 轴转向相机（Z 轴向上约定）
//
//  变换模型：世界矩阵 = 父世界矩阵 × T·R·S × 绘制项局部矩阵
//  R 是四元数（Eigen），增量旋转 rotate(axis, angle) 不会产生万向死锁；
//  每帧由场景调用 updateWorld(parentWorld, cameraPosition) 自顶向下重算。
// ============================================================================

class YALN_CORE_EXPORT YalnNode
{
public:
    enum class Type {
        Normal, Group, Leaf,
        Translate, Rotate, Scale, Spin,      // 变换节点族
        Switch, Billboard                     // 行为节点
    };

    explicit YalnNode(std::string name = "Node");
    virtual ~YalnNode() = default;

    YalnNode(const YalnNode&)            = delete;
    YalnNode& operator=(const YalnNode&) = delete;

    virtual Type nodeType() const { return Type::Normal; }

    // ---- 树结构 ----
    // 叶子节点拒绝子节点；同名子节点拒绝（find 依赖名字唯一性）
    bool addChild(std::unique_ptr<YalnNode> child);

    // 移除并销毁指定名字的子节点（找不到返回 false）
    bool removeChild(const std::string& name);

    // 组节点拒绝绘制项
    bool attachDrawable(std::unique_ptr<YalnDrawable> drawable);

    // 移除指定下标的绘制项（越界返回 false）
    bool detachDrawable(size_t index);

    YalnNode* find(const std::string& name);   // 深度优先，含自身

    YalnNode* parent() const { return m_parent; }
    const std::string& name() const { return m_name; }
    void rename(const std::string& name) { m_name = name; }

    size_t childCount()    const { return m_children.size(); }
    size_t drawableCount() const { return m_drawables.size(); }

    // 递归统计整棵子树的节点数 / 绘制项数
    size_t countNodes() const;
    size_t countDrawables() const;

    const std::vector<std::unique_ptr<YalnNode>>&     children()  const { return m_children; }
    const std::vector<std::unique_ptr<YalnDrawable>>& drawables() const { return m_drawables; }

    // 递归设置整棵子树的材质 / Pass 掩码（便于按组切换外观）
    void setMaterialRecursive(YalnMaterialHandle material)
    {
        for (const auto& drawable : m_drawables)
        {
            if (drawable != nullptr)
            {
                drawable->setMaterial(material);
            }
        }
        for (const auto& child : m_children)
        {
            if (child != nullptr)
            {
                child->setMaterialRecursive(material);
            }
        }
    }

    void setPassMaskRecursive(YalnPassMask passMask)
    {
        for (const auto& drawable : m_drawables)
        {
            if (drawable != nullptr)
            {
                drawable->setPassMask(passMask);
            }
        }
        for (const auto& child : m_children)
        {
            if (child != nullptr)
            {
                child->setPassMaskRecursive(passMask);
            }
        }
    }

    void setPipelineOverrideRecursive(YalnPipelineHandle pipeline)
    {
        for (const auto& drawable : m_drawables)
        {
            if (drawable != nullptr)
            {
                drawable->setPipelineOverride(pipeline);
            }
        }
        for (const auto& child : m_children)
        {
            if (child != nullptr)
            {
                child->setPipelineOverrideRecursive(pipeline);
            }
        }
    }

    // 递归给整棵子树的所有 drawable 添加/清空叠加材质
    void addOverlayMaterialRecursive(YalnMaterialHandle material)
    {
        for (const auto& drawable : m_drawables)
        {
            if (drawable != nullptr)
            {
                drawable->addOverlayMaterial(material);
            }
        }
        for (const auto& child : m_children)
        {
            if (child != nullptr)
            {
                child->addOverlayMaterialRecursive(material);
            }
        }
    }

    void clearOverlayMaterialsRecursive()
    {
        for (const auto& drawable : m_drawables)
        {
            if (drawable != nullptr)
            {
                drawable->clearOverlayMaterials();
            }
        }
        for (const auto& child : m_children)
        {
            if (child != nullptr)
            {
                child->clearOverlayMaterialsRecursive();
            }
        }
    }

    // ---- 本节点变换（位置/缩放 Eigen 向量，朝向 Eigen 四元数）----

    void setPosition(const Eigen::Vector3f& p) { m_position = p; }
    void setScale(const Eigen::Vector3f& s)    { m_scale = s; }

    // 朝向整体赋值（四元数，自动归一化）
    void setOrientation(const Eigen::Quaternionf& q) { m_orientation = q.normalized(); }

    // 欧拉角便捷入口（弧度，yaw/pitch/roll 分量，本征序 ZYX）
    // 内部立即转为四元数存储；仅建议用于初始化，动画请用 rotate()
    void setEulerRotation(const Eigen::Vector3f& eulerYPR)
    {
        m_orientation = eulerToQuat(eulerYPR);
    }

    // 增量旋转（本节点局部空间，绕自身轴）：四元数右乘，任意角度无死锁
    void rotate(const Eigen::Vector3f& axis, float angleRadians)
    {
        m_orientation = (m_orientation * axisAngleToQuat(axis, angleRadians)).normalized();
    }

    // 增量旋转（父级/世界空间）：四元数左乘
    void rotateWorld(const Eigen::Vector3f& axis, float angleRadians)
    {
        m_orientation = (axisAngleToQuat(axis, angleRadians) * m_orientation).normalized();
    }

    const Eigen::Vector3f&    position()    const { return m_position; }
    const Eigen::Vector3f&    scale()       const { return m_scale; }
    const Eigen::Quaternionf& orientation() const { return m_orientation; }

    // 局部矩阵：普通节点 = T·R·S；变换/行为节点覆写自己的语义
    virtual glm::mat4 localMatrix() const;

    const glm::mat4& worldMatrix() const { return m_worldMatrix; }

    // ---- 遍历 ----
    // cameraPosition：供公告板等行为节点使用（普通节点忽略）
    virtual void updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition);
    virtual void collectInstances(std::vector<YalnDrawableInstance>& out) const;  // 收集可见绘制实例

protected:
    // 类型约束的开关（组/叶子通过覆写收紧）
    virtual bool acceptsChildren()  const { return true; }
    virtual bool acceptsDrawables() const { return true; }

    YalnNode* m_parent = nullptr;
    std::string m_name;

    std::vector<std::unique_ptr<YalnNode>>     m_children;
    std::vector<std::unique_ptr<YalnDrawable>> m_drawables;

    Eigen::Vector3f       m_position{ Eigen::Vector3f::Zero() };
    Eigen::Quaternionf    m_orientation{ Eigen::Quaternionf::Identity() };
    Eigen::Vector3f       m_scale{ Eigen::Vector3f::Ones() };

    glm::mat4 m_worldMatrix{ 1.0f };
};

// ============================================================================
//  YalnGroupNode —— 组节点：纯组织容器，只接受子节点
// ============================================================================
class YALN_CORE_EXPORT YalnGroupNode : public YalnNode
{
public:
    explicit YalnGroupNode(std::string name = "Group")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Group; }

protected:
    bool acceptsDrawables() const override { return false; }
};

// ============================================================================
//  YalnLeafNode —— 叶子节点：只接受可绘制对象
// ============================================================================
class YALN_CORE_EXPORT YalnLeafNode : public YalnNode
{
public:
    explicit YalnLeafNode(std::string name = "Leaf")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Leaf; }

protected:
    bool acceptsChildren() const override { return false; }
};

// ============================================================================
//  变换节点族 —— 语义聚焦的便捷节点，链式组合出复杂装配：
//
//    TranslateNode("pos") → RotateNode("tilt") → ScaleNode("size") → 叶子
//
//  约定：localMatrix 统一为 T·R·S（继承基类）；"纯位移/纯旋转/纯缩放"
//  是使用约定——只设置对应分量，其余保持默认（零位移/恒等朝向/单位缩放）。
//  在节点上额外设置其它分量同样有效（如 SpinNode 用 setPosition 抬高挂点）。
// ============================================================================

// 位移节点：语义别名 + 只关注 position 的使用约定
class YALN_CORE_EXPORT YalnTranslateNode : public YalnNode
{
public:
    explicit YalnTranslateNode(std::string name = "Translate")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Translate; }

    // 语义别名：位移节点的"偏移量"就是 base 的 position
    void setOffset(const Eigen::Vector3f& offset) { setPosition(offset); }
    const Eigen::Vector3f& offset() const { return position(); }
};

// 旋转节点：轴角便捷入口（四元数存储，增量旋转无万向死锁）
class YALN_CORE_EXPORT YalnRotateNode : public YalnNode
{
public:
    explicit YalnRotateNode(std::string name = "Rotate")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Rotate; }

    // 一次设置轴角旋转（自动归一化轴）
    void setAxisAngle(const Eigen::Vector3f& axis, float angleRadians)
    {
        setOrientation(axisAngleToQuat(axis, angleRadians));
    }

    // 增量旋转走 base 的 rotate()/rotateWorld()（四元数复合，无死锁）
    // 兼容旧欧拉入口
    void setAxis(const Eigen::Vector3f& axis)    { m_lastAxis = axis; }
    void setAngle(float angleRadians)            { setAxisAngle(m_lastAxis, angleRadians); }

private:
    Eigen::Vector3f m_lastAxis{ Eigen::Vector3f::UnitZ() };   // setAngle 单独调用时的轴
};

// 缩放节点：非均匀/均匀缩放便捷入口
class YALN_CORE_EXPORT YalnScaleNode : public YalnNode
{
public:
    explicit YalnScaleNode(std::string name = "Scale")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Scale; }

    // 非均匀缩放；uniform 便捷入口
    void setScaleFactor(const Eigen::Vector3f& factor) { setScale(factor); }
    void setUniformScale(float factor)                 { setScale(Eigen::Vector3f::Constant(factor)); }

    const Eigen::Vector3f& scaleFactor() const { return scale(); }
};

// ============================================================================
//  YalnSpinNode —— 自旋动画节点：绕轴持续旋转（四元数按 dt 累乘）
//
//  适合风车叶轮、装饰物等持续动画。时间由内部稳定时钟驱动。
//  四元数累乘不经过欧拉角，连续自旋不存在姿态翻转或死区。
//  挂点高度等用 setPosition 设置（localMatrix 统一 T·R·S）。
// ============================================================================
class YALN_CORE_EXPORT YalnSpinNode : public YalnNode
{
public:
    explicit YalnSpinNode(std::string name = "Spin")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Spin; }

    void setAxis(const Eigen::Vector3f& axis) // 自旋轴（自动归一化）
    {
        const float len = axis.norm();
        m_axis = len > 1e-6f ? Eigen::Vector3f(axis / len) : Eigen::Vector3f::UnitZ();
    }
    void setSpeed(float radiansPerSecond)     { m_speed = radiansPerSecond; }
    void setEnabled(bool enabled)             { m_enabled = enabled; }   // false = 暂停
    void reset()                              { setOrientation(Eigen::Quaternionf::Identity()); m_lastTime.reset(); }
    void setOrientationAngle(float angleRadians)  // 直接设定当前累计角度
    {
        setOrientation(axisAngleToQuat(m_axis, angleRadians));
    }
    float angle() const   // 从四元数反解当前累计角度（绕轴的有符号角）
    {
        return 2.0f * std::atan2(
            m_orientation.dot(Eigen::Quaternionf(0.0f, m_axis.x(), m_axis.y(), m_axis.z())),
            m_orientation.w());
    }

    const Eigen::Vector3f& axis() const { return m_axis; }
    float speed()         const { return m_speed; }
    bool  isEnabled()     const { return m_enabled; }

    void updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition) override;

private:
    Eigen::Vector3f m_axis{ Eigen::Vector3f::UnitZ() };
    float     m_speed   = 1.5707963f;   // 默认每秒 90°
    bool      m_enabled = true;

    std::optional<std::chrono::steady_clock::time_point> m_lastTime;
};

// ============================================================================
//  YalnSwitchNode —— 开关节点：多个子节点只激活一个
// ============================================================================
class YALN_CORE_EXPORT YalnSwitchNode : public YalnNode
{
public:
    explicit YalnSwitchNode(std::string name = "Switch")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Switch; }

    // 越界时拒绝并保持原激活项；-1 = 全部关闭
    bool setActiveChild(int index);
    bool setActiveChild(const std::string& name);
    int  activeIndex() const { return m_activeIndex; }

    void updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition) override;
    void collectInstances(std::vector<YalnDrawableInstance>& out) const override;

private:
    int m_activeIndex = 0;   // 默认激活第一个子节点；无子节点时等于关闭
};

// ============================================================================
//  YalnBillboardNode —— 圆柱公告板节点：绕 Z 轴转向相机
//
//  每帧根据相机水平方位角组装朝向（轴角→四元数）。
//  绘制几何需要法线朝 +X 的薄片（如 YalnCube(0.05, w, h)）。
// ============================================================================
class YALN_CORE_EXPORT YalnBillboardNode : public YalnNode
{
public:
    explicit YalnBillboardNode(std::string name = "Billboard")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Billboard; }

    void setEnabled(bool enabled) { m_enabled = enabled; }   // false = 冻结当前朝向
    bool isEnabled()        const { return m_enabled; }
    float facingAngle()     const { return m_facingAngle; }

    void updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition) override;

private:
    bool  m_enabled = true;
    float m_facingAngle = 0.0f;   // 缓存上次朝向（相机与节点重合时保持）
};

#endif // YALN_NODE_H

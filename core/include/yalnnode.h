#ifndef YALN_NODE_H
#define YALN_NODE_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "global_type.h"
#include "yalndrawable.h"

// ============================================================================
//  场景树节点体系
//
//  结构节点：
//    YalnNode       普通节点：可以包含子节点，也可以包含可绘制对象
//    YalnGroupNode  组节点：只能包含子节点（纯组织用途，挂绘制项被拒绝）
//    YalnLeafNode   叶子节点：只能包含可绘制对象（加子节点被拒绝）
//    YalnSwitchNode 开关节点：多个子节点只激活一个（多状态/多方案切换）
//
//  变换节点族（语义单一，方便动画与装配；各自只取一个变换分量）：
//    YalnTranslateNode 纯位移    YalnRotateNode 纯旋转（轴 + 角度）
//    YalnScaleNode     纯缩放    YalnSpinNode   绕轴持续自旋（动画）
//
//  行为节点：
//    YalnBillboardNode 圆柱公告板：绕 Z 轴转向相机（Z 轴向上约定）
//
//  变换模型：世界矩阵 = 父世界矩阵 × 本节点 localMatrix() × 绘制项局部 TRS
//  每帧由场景调用 updateWorld(parentWorld, cameraPosition) 自顶向下重算。
//
//  约束执行：叶子/组节点的限制是运行期契约——违反时拒绝并给出原因。
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

    // ---- 本节点 TRS（变换节点族会选择性使用其中某分量，见各类说明）----
    void setPosition(const glm::vec3& p) { m_position = p; }
    void setRotation(const glm::vec3& r) { m_rotation = r; }   // 弧度
    void setScale(const glm::vec3& s)    { m_scale = s; }

    const glm::vec3& position() const { return m_position; }
    const glm::vec3& rotation() const { return m_rotation; }
    const glm::vec3& scale()    const { return m_scale; }

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

    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 m_rotation{ 0.0f, 0.0f, 0.0f };
    glm::vec3 m_scale{ 1.0f, 1.0f, 1.0f };

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
//  变换节点族 —— 每个节点只负责一个变换分量，链式组合出复杂装配：
//
//    TranslateNode("pos") → RotateNode("tilt") → ScaleNode("size") → 叶子
//
//  各类只取 base TRS 中属于自己的分量，其余分量被忽略（localMatrix 覆写）。
// ============================================================================

// 纯位移：localMatrix = T（旋转/缩放分量被忽略）
class YALN_CORE_EXPORT YalnTranslateNode : public YalnNode
{
public:
    explicit YalnTranslateNode(std::string name = "Translate")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Translate; }

    // 语义别名：位移节点的"偏移量"就是 base 的 position
    void setOffset(const glm::vec3& offset) { setPosition(offset); }
    const glm::vec3& offset() const { return position(); }

    glm::mat4 localMatrix() const override;
};

// 纯旋转：localMatrix = R(angle, axis)（position/scale 分量被忽略）
class YALN_CORE_EXPORT YalnRotateNode : public YalnNode
{
public:
    explicit YalnRotateNode(std::string name = "Rotate")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Rotate; }

    void setAxis(const glm::vec3& axis)
    {
        const float len = glm::length(axis);
        m_axis = len > 1e-6f ? axis / len : glm::vec3(0.0f, 0.0f, 1.0f);   // 归一化，零向量兜底
    }
    void setAngle(float angleRadians)      { m_angle = angleRadians; }

    const glm::vec3& axis()  const { return m_axis; }
    float            angle() const { return m_angle; }

    glm::mat4 localMatrix() const override;

private:
    glm::vec3 m_axis{ 0.0f, 0.0f, 1.0f };
    float     m_angle = 0.0f;
};

// 纯缩放：localMatrix = S（position/rotation 分量被忽略）
class YALN_CORE_EXPORT YalnScaleNode : public YalnNode
{
public:
    explicit YalnScaleNode(std::string name = "Scale")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Scale; }

    // 非均匀缩放；uniform 便捷入口
    void setScaleFactor(const glm::vec3& factor) { m_scaleFactor = factor; }
    void setUniformScale(float factor)           { m_scaleFactor = glm::vec3(factor); }

    const glm::vec3& scaleFactor() const { return m_scaleFactor; }

    glm::mat4 localMatrix() const override;

private:
    glm::vec3 m_scaleFactor{ 1.0f, 1.0f, 1.0f };
};

// ============================================================================
//  YalnSpinNode —— 自旋动画节点：绕轴持续旋转
//
//  适合风车叶轮、装饰物等持续动画。时间由内部稳定时钟驱动
//  （每帧 updateWorld 时累加 dt），暂停/恢复/复位/调速随时可调。
// ============================================================================
class YALN_CORE_EXPORT YalnSpinNode : public YalnNode
{
public:
    explicit YalnSpinNode(std::string name = "Spin")
        : YalnNode(std::move(name)) {}

    Type nodeType() const override { return Type::Spin; }

    void setAxis(const glm::vec3& axis)
    {
        const float len = glm::length(axis);
        m_axis = len > 1e-6f ? axis / len : glm::vec3(0.0f, 0.0f, 1.0f);
    }
    void setSpeed(float radiansPerSecond)     { m_speed = radiansPerSecond; }
    void setEnabled(bool enabled)             { m_enabled = enabled; }   // false = 暂停
    void reset()                              { m_angle = 0.0f; m_lastTime.reset(); }
    void setAngle(float angleRadians)         { m_angle = angleRadians; }

    const glm::vec3& axis() const { return m_axis; }
    float speed()         const { return m_speed; }
    bool  isEnabled()     const { return m_enabled; }
    float angle()         const { return m_angle; }

    glm::mat4 localMatrix() const override;

    void updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition) override;

private:
    glm::vec3 m_axis{ 0.0f, 0.0f, 1.0f };
    float     m_speed   = glm::radians(90.0f);   // 默认每秒 90°
    bool      m_enabled = true;
    float     m_angle   = 0.0f;

    std::optional<std::chrono::steady_clock::time_point> m_lastTime;
};

// ============================================================================
//  YalnSwitchNode —— 开关节点：多个子节点只激活一个
//
//  经典场景图节点（多状态/多方案切换，如 LOD、日夜、装备变体）。
//  updateWorld 与 collectInstances 都只遍历激活的子节点——
//  未激活子树零开销（不更新矩阵、不产生绘制项）。
//  自身仍可挂绘制项（激活态下的公共部分）。
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
//  Z 轴向上约定下做"水平朝向"的公告板（标记牌、树冠、粒子片）：
//  每帧根据相机水平方位绕 Z 轴旋转。绘制几何需要法线朝 +X 的薄片
//  （如 YalnCube(0.05, w, h)），旋转后薄片正面正对相机。
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

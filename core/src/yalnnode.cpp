#include "../include/yalnnode.h"

#include <algorithm>
#include <cmath>
#include <iostream>

// ==================== YalnNode（基类） ====================

YalnNode::YalnNode(std::string name)
    : m_name(std::move(name))
{
}

glm::mat4 YalnNode::localMatrix() const
{
    glm::mat4 m(1.0f);
    m = glm::translate(m, m_position);
    m = glm::rotate(m, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::rotate(m, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    return glm::scale(m, m_scale);
}

bool YalnNode::addChild(std::unique_ptr<YalnNode> child)
{
    if (child == nullptr)
    {
        std::cerr << "[YalnNode:" << m_name << "] addChild: 不能添加空节点" << std::endl;
        return false;
    }

    if (!acceptsChildren())
    {
        std::cerr << "[YalnNode:" << m_name << "] 叶子节点只能包含可绘制对象，"
                  << "拒绝子节点 \"" << child->name() << "\"" << std::endl;
        return false;
    }

    if (child.get() == this)
    {
        std::cerr << "[YalnNode:" << m_name << "] addChild: 不能把节点挂到自己下面" << std::endl;
        return false;
    }

    // 同名子节点拒绝：find/removeChild 依赖名字唯一
    if (find(child->name()) != nullptr)
    {
        std::cerr << "[YalnNode:" << m_name << "] 子树中已存在同名节点 \""
                  << child->name() << "\"，拒绝添加" << std::endl;
        return false;
    }

    child->m_parent = this;
    m_children.push_back(std::move(child));
    return true;
}

bool YalnNode::removeChild(const std::string& name)
{
    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if ((*it)->name() == name)
        {
            (*it)->m_parent = nullptr;
            m_children.erase(it);
            return true;
        }
    }
    return false;
}

bool YalnNode::attachDrawable(std::unique_ptr<YalnDrawable> drawable)
{
    if (drawable == nullptr)
    {
        std::cerr << "[YalnNode:" << m_name << "] attachDrawable: 不能添加空绘制项" << std::endl;
        return false;
    }

    if (!acceptsDrawables())
    {
        std::cerr << "[YalnNode:" << m_name << "] 组节点只能包含子节点，拒绝绘制项" << std::endl;
        return false;
    }

    m_drawables.push_back(std::move(drawable));
    return true;
}

bool YalnNode::detachDrawable(size_t index)
{
    if (index >= m_drawables.size())
    {
        return false;
    }
    m_drawables.erase(m_drawables.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

YalnNode* YalnNode::find(const std::string& name)
{
    if (m_name == name)
    {
        return this;
    }
    for (const auto& child : m_children)
    {
        if (YalnNode* found = child->find(name))
        {
            return found;
        }
    }
    return nullptr;
}

size_t YalnNode::countNodes() const
{
    size_t count = 1;
    for (const auto& child : m_children)
    {
        count += child->countNodes();
    }
    return count;
}

size_t YalnNode::countDrawables() const
{
    size_t count = m_drawables.size();
    for (const auto& child : m_children)
    {
        count += child->countDrawables();
    }
    return count;
}

void YalnNode::updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition)
{
    (void)cameraPosition;   // 基类不使用（公告板等覆写使用）
    m_worldMatrix = parentWorld * localMatrix();
    for (const auto& child : m_children)
    {
        child->updateWorld(m_worldMatrix, cameraPosition);
    }
}

void YalnNode::collectInstances(std::vector<YalnDrawableInstance>& out) const
{
    for (const auto& drawable : m_drawables)
    {
        if (drawable->visible() && drawable->mesh() != nullptr)
        {
            out.push_back({ drawable.get(), m_worldMatrix * drawable->localMatrix() });
        }
    }
    for (const auto& child : m_children)
    {
        child->collectInstances(out);
    }
}

// ==================== 变换节点族 ====================

glm::mat4 YalnTranslateNode::localMatrix() const
{
    return glm::translate(glm::mat4(1.0f), m_position);
}

glm::mat4 YalnRotateNode::localMatrix() const
{
    return glm::rotate(glm::mat4(1.0f), m_angle, m_axis);
}

glm::mat4 YalnScaleNode::localMatrix() const
{
    return glm::scale(glm::mat4(1.0f), m_scaleFactor);
}

// ==================== YalnSpinNode ====================

glm::mat4 YalnSpinNode::localMatrix() const
{
    return glm::rotate(glm::mat4(1.0f), m_angle, m_axis);
}

void YalnSpinNode::updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition)
{
    (void)cameraPosition;

    // 用内部稳定时钟累加角度（暂停期间不积累时间）
    if (m_enabled && m_speed != 0.0f)
    {
        const auto now = std::chrono::steady_clock::now();
        if (m_lastTime.has_value())
        {
            const float dt = std::chrono::duration<float>(now - *m_lastTime).count();
            m_angle += m_speed * std::min(dt, 0.1f);   // 卡顿/断点时防跳变
        }
        m_lastTime = now;
    }
    else
    {
        m_lastTime.reset();   // 恢复时从当前时刻重新计时
    }

    m_worldMatrix = parentWorld * localMatrix();
    for (const auto& child : m_children)
    {
        child->updateWorld(m_worldMatrix, cameraPosition);
    }
}

// ==================== YalnSwitchNode ====================

bool YalnSwitchNode::setActiveChild(int index)
{
    if (index != -1 && (index < 0 || static_cast<size_t>(index) >= m_children.size()))
    {
        std::cerr << "[YalnSwitchNode:" << m_name << "] 激活下标越界 (" << index
                  << "，共 " << m_children.size() << " 个子节点)" << std::endl;
        return false;
    }
    m_activeIndex = index;
    return true;
}

bool YalnSwitchNode::setActiveChild(const std::string& name)
{
    for (size_t i = 0; i < m_children.size(); i++)
    {
        if (m_children[i]->name() == name)
        {
            m_activeIndex = static_cast<int>(i);
            return true;
        }
    }
    std::cerr << "[YalnSwitchNode:" << m_name << "] 找不到子节点 \"" << name << "\"" << std::endl;
    return false;
}

void YalnSwitchNode::updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition)
{
    m_worldMatrix = parentWorld * localMatrix();

    // 只更新激活分支（未激活子树保持旧矩阵且不产生绘制项，零开销）
    if (m_activeIndex >= 0 && static_cast<size_t>(m_activeIndex) < m_children.size())
    {
        m_children[static_cast<size_t>(m_activeIndex)]
            ->updateWorld(m_worldMatrix, cameraPosition);
    }
}

void YalnSwitchNode::collectInstances(std::vector<YalnDrawableInstance>& out) const
{
    for (const auto& drawable : m_drawables)
    {
        if (drawable->visible() && drawable->mesh() != nullptr)
        {
            out.push_back({ drawable.get(), m_worldMatrix * drawable->localMatrix() });
        }
    }

    if (m_activeIndex >= 0 && static_cast<size_t>(m_activeIndex) < m_children.size())
    {
        m_children[static_cast<size_t>(m_activeIndex)]->collectInstances(out);
    }
}

// ==================== YalnBillboardNode ====================

void YalnBillboardNode::updateWorld(const glm::mat4& parentWorld, const glm::vec3& cameraPosition)
{
    if (m_enabled)
    {
        // 节点世界位置 = 父世界矩阵作用在本地位移上
        const glm::vec3 myWorldPos(parentWorld * glm::vec4(m_position, 1.0f));
        const glm::vec3 toCamera = cameraPosition - myWorldPos;

        // 水平方位角：绕 Z 轴把 +X 转向相机（几何需为法线朝 +X 的薄片）
        if (glm::length(glm::vec2(toCamera.x, toCamera.y)) > 1e-4f)
        {
            m_facingAngle = std::atan2(toCamera.y, toCamera.x);
        }
        // 相机与节点水平重合时保持上次朝向
    }

    glm::mat4 local = glm::translate(glm::mat4(1.0f), m_position);
    local = glm::rotate(local, m_facingAngle, glm::vec3(0.0f, 0.0f, 1.0f));
    local = glm::scale(local, m_scale);

    m_worldMatrix = parentWorld * local;
    for (const auto& child : m_children)
    {
        child->updateWorld(m_worldMatrix, cameraPosition);
    }
}

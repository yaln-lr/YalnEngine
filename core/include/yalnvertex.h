#ifndef YALN_VERTEX_H
#define YALN_VERTEX_H

#include "global_type.h"
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

// 顶点数据
struct YALN_CORE_EXPORT YalnVertexPC {
public:
    glm::vec3 pos;    // 位置
    glm::vec3 color;  // 颜色

    // 获取顶点绑定描述（定义如何将顶点数据绑定到管线）
    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;                            // 绑定点索引
        bindingDescription.stride = sizeof(YalnVertexPC);                // 每个顶点的数据大小
        bindingDescription.inputRate = vk::VertexInputRate::eVertex; // 每个顶步进
        return bindingDescription;
    }

    // 获取顶点属性描述（定义顶点数据的格式）
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

        // 位置属性
        attributeDescriptions[0].binding = 0;                       // 绑定点
        attributeDescriptions[0].location = 0;                     // 着色器中的位置
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;  // 格式（3个浮点数）
        attributeDescriptions[0].offset = offsetof(YalnVertexPC, pos);  // 偏移量

        // 颜色属性
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(YalnVertexPC, color);  // 偏移量

        return attributeDescriptions;
    }
};
#endif // YALN_VERTEX_H
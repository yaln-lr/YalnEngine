#include "../include/yalnmodematerial.h"

#include "yalninterfaces.h"
#include "yalnrendertypes.h"

#include <iostream>

// ============================================================================
//  YalnWhiteModelMaterial
// ============================================================================

YalnWhiteModelMaterial::YalnWhiteModelMaterial(YalnView& view)
    : YalnModeMaterial(view)
{
}

YalnWhiteModelMaterial::~YalnWhiteModelMaterial() = default;

bool YalnWhiteModelMaterial::create()
{
    if (isReady())
    {
        return true;
    }

    // 白膜 shader：默认顶点着色器 + 白膜专用片元着色器（忽略顶点色）。
    // 路径相对于 viewer 运行目录（build/mingw/viewer）。
    m_shaderProgram = std::make_unique<YalnSpirvShaderProgram>(
        "../shaders/shader.vert.spv",
        "../shaders/white.frag.spv");
    if (!m_shaderProgram->load(m_view.device()))
    {
        std::cerr << "[WhiteModelMaterial] 着色器加载失败" << std::endl;
        m_shaderProgram.reset();
        return false;
    }

    YalnPipelineConfig config = m_view.pipeline().config();
    m_pipeline = m_view.createPipeline(config, *m_shaderProgram, "RenderMode.WhiteModel");
    if (m_pipeline == YALN_INVALID_PIPELINE)
    {
        std::cerr << "[WhiteModelMaterial] PSO 创建失败" << std::endl;
        return false;
    }

    YalnMaterialDesc desc;
    desc.name = name();
    desc.baseColor = glm::vec4(1.0f);
    desc.passMask = toPassMask(YalnPassKind::Opaque);
    desc.defaultPipeline = m_pipeline;
    m_handle = m_view.createMaterial(desc);
    if (m_handle == YALN_INVALID_MATERIAL)
    {
        std::cerr << "[WhiteModelMaterial] 材质登记失败" << std::endl;
        return false;
    }
    return true;
}

// ============================================================================
//  YalnWireframeMaterial
// ============================================================================

YalnWireframeMaterial::YalnWireframeMaterial(YalnView& view)
    : YalnModeMaterial(view)
{
}

bool YalnWireframeMaterial::create()
{
    if (isReady())
    {
        return true;
    }

    if (!m_view.device().physicalDevice().getFeatures().fillModeNonSolid)
    {
        std::cerr << "[WireframeMaterial] 设备不支持 fillModeNonSolid" << std::endl;
        return false;
    }

    YalnPipelineConfig config = m_view.pipeline().config();
    config.polygonMode = vk::PolygonMode::eLine;
    config.cullMode = vk::CullModeFlagBits::eNone;
    config.lineWidth = 1.0f;

    // 叠加在填充之上：关闭深度写入 + 负深度偏移，避免共面 z-fighting。
    config.depthBiasEnable = true;
    config.depthBiasConstantFactor = -1.5f;
    config.depthBiasSlopeFactor = -1.5f;
    config.depthWrite = false;

    m_pipeline = m_view.createPipeline(config, "RenderMode.Wireframe");
    if (m_pipeline == YALN_INVALID_PIPELINE)
    {
        std::cerr << "[WireframeMaterial] PSO 创建失败" << std::endl;
        return false;
    }

    YalnMaterialDesc desc;
    desc.name = name();
    desc.baseColor = glm::vec4(0.35f, 1.0f, 0.65f, 1.0f);  // 青绿色线框调制
    // Opake + Outline：既能作主材质画线框，也能作为叠加材质叠加绘制。
    desc.passMask = toPassMask(YalnPassKind::Opaque) |
                    toPassMask(YalnPassKind::Outline);
    desc.defaultPipeline = m_pipeline;
    m_handle = m_view.createMaterial(desc);
    if (m_handle == YALN_INVALID_MATERIAL)
    {
        std::cerr << "[WireframeMaterial] 材质登记失败" << std::endl;
        return false;
    }
    return true;
}

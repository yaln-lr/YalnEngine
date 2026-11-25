#ifndef YALN_MODE_MATERIAL_H
#define YALN_MODE_MATERIAL_H

#include <memory>

#include "global_type.h"
#include "yalnrendertypes.h"
#include "yalnshaderprogram.h"
#include "yalnview.h"

// ============================================================================
//  YalnModeMaterial —— 渲染模式专用材质的抽象
//
//  这是一层“包装类”，不是 YalnMaterial 的子类：
//  YalnMaterial 由 YalnMaterialLibrary 以 unique_ptr<YalnMaterial> 持有且析构非虚，
//  因此不能安全继承后放进库里。特殊材质类负责：
//    1. 创建（或复用）自己专用的 PSO；
//    2. 用 YalnView::createMaterial() 在材质库中登记一个材质；
//    3. 对外只暴露材质句柄，供 drawable 的 setMaterial / addOverlayMaterial 使用。
//
//  GPU/PSO/shader 的生命周期由本类持有，句柄在材质库 clear() 前有效。
// ============================================================================
class YalnModeMaterial
{
public:
    virtual ~YalnModeMaterial() = default;

    virtual const char* name() const = 0;

    // 创建专用 PSO + 材质（幂等；首次成功后不再重建）。
    virtual bool create() = 0;

    bool isReady() const { return m_handle != YALN_INVALID_MATERIAL; }

    YalnMaterialHandle handle() const { return m_handle; }
    YalnPipelineHandle pipeline() const { return m_pipeline; }

protected:
    explicit YalnModeMaterial(YalnView& view) : m_view(view) {}

    YalnView& m_view;
    YalnPipelineHandle m_pipeline = YALN_INVALID_PIPELINE;
    YalnMaterialHandle m_handle   = YALN_INVALID_MATERIAL;
};

// ============================================================================
//  YalnWhiteModelMaterial —— 白膜材质
//
//  使用白膜专用片元着色器（忽略顶点色，输出中性白反照率 + 光照），
//  只在 Opaque Pass 绘制（passMask = Opaque）。
// ============================================================================
class YalnWhiteModelMaterial : public YalnModeMaterial
{
public:
    explicit YalnWhiteModelMaterial(YalnView& view);
    ~YalnWhiteModelMaterial() override;

    const char* name() const override { return "Mode.White"; }
    bool create() override;

private:
    std::unique_ptr<YalnSpirvShaderProgram> m_shaderProgram;
};

// ============================================================================
//  YalnWireframeMaterial —— 网格（线框）材质
//
//  使用线框 PSO（PolygonMode::eLine + depthBias + 不写深度），
//  passMask 同时含 Opaque 与 Outline：既能作主材质画线框，
//  也能作为叠加材质在 YalnMaterialOverlayPass(Outline) 中叠加。
// ============================================================================
class YalnWireframeMaterial : public YalnModeMaterial
{
public:
    explicit YalnWireframeMaterial(YalnView& view);

    const char* name() const override { return "Mode.Wire"; }
    bool create() override;
};

#endif // YALN_MODE_MATERIAL_H

#include "../include/yalnforwardpass.h"
#include "../include/yalninterfaces.h"
#include "../include/yalnmateriallibrary.h"
#include "../include/yalnpipelinelibrary.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace
{
struct ResolvedItem
{
    const YalnDrawItem* item = nullptr;
    const YalnMaterial* material = nullptr;
    YalnMaterialHandle materialHandle = YALN_INVALID_MATERIAL;
    const IYalnPipeline* pipeline = nullptr;
    YalnPipelineHandle pipelineHandle = YALN_DEFAULT_PIPELINE;
};

enum class MaterialSource
{
    Primary,   // item.material
    Overlay    // item.overlayMaterials[]
};

// OpaquePass 与 MaterialOverlayPass 共用的批处理实现。
// 两者在 Vulkan 层共享同一个 RenderPass，只是数据来源和 passMask 不同。
void recordMaterialPass(const YalnFrameContext& context,
                        YalnPassKind           pass,
                        MaterialSource         source)
{
    if (context.commandBuffer == VK_NULL_HANDLE || context.scene == nullptr)
    {
        return;
    }

    std::vector<ResolvedItem> resolved;

    auto resolve = [&](const YalnDrawItem& item, YalnMaterialHandle handle)
    {
        if (item.gpuMesh == nullptr || !item.passMask.contains(pass))
        {
            return;
        }

        const YalnMaterial* material = nullptr;
        YalnMaterialHandle materialHandle = handle;
        if (context.materials != nullptr)
        {
            material = context.materials->get(handle);
            if (material == nullptr)
            {
                if (source == MaterialSource::Overlay)
                {
                    // 无效叠加材质：直接跳过，不回退默认材质（否则会意外重画一遍）
                    return;
                }
                material = context.materials->get(YALN_DEFAULT_MATERIAL);
                materialHandle = YALN_DEFAULT_MATERIAL;
            }
        }

        // 材质自身的 passMask 同样必须包含当前 Pass 位
        if (material != nullptr && !material->passMask().contains(pass))
        {
            return;
        }

        // pipelineOverride 仅主材质路径 honor；叠加材质完全由材质自身决定。
        YalnPipelineHandle pipelineHandle = YALN_INVALID_PIPELINE;
        if (source == MaterialSource::Primary)
        {
            pipelineHandle = item.pipelineOverride;
        }
        if (pipelineHandle == YALN_INVALID_PIPELINE)
        {
            pipelineHandle = material != nullptr
                ? material->pipelineForPass(pass)
                : YALN_DEFAULT_PIPELINE;
        }

        const IYalnPipeline* pipeline = context.pipelines != nullptr
            ? context.pipelines->get(pipelineHandle)
            : nullptr;
        YalnPipelineHandle resolvedHandle = pipelineHandle;

        if (pipeline == nullptr && context.pipelines != nullptr)
        {
            pipeline = context.pipelines->get(YALN_DEFAULT_PIPELINE);
            resolvedHandle = YALN_DEFAULT_PIPELINE;
        }

        if (pipeline == nullptr || !pipeline->isValid())
        {
            return;
        }

        ResolvedItem entry;
        entry.item           = &item;
        entry.material       = material;
        entry.materialHandle = materialHandle;
        entry.pipeline       = pipeline;
        entry.pipelineHandle = resolvedHandle;
        resolved.push_back(entry);
    };

    for (const YalnDrawItem& item : context.scene->drawItems())
    {
        if (source == MaterialSource::Primary)
        {
            resolve(item, item.material);
        }
        else
        {
            for (const YalnMaterialHandle handle : item.overlayMaterials)
            {
                resolve(item, handle);
            }
        }
    }

    if (resolved.empty())
    {
        return;
    }

    // 批处理：pipeline → material → mesh
    std::sort(resolved.begin(), resolved.end(),
              [](const ResolvedItem& a, const ResolvedItem& b)
              {
                  if (a.pipelineHandle != b.pipelineHandle)
                  {
                      return a.pipelineHandle < b.pipelineHandle;
                  }
                  if (a.materialHandle != b.materialHandle)
                  {
                      return a.materialHandle < b.materialHandle;
                  }
                  return std::less<const YalnGpuMesh*>()(a.item->gpuMesh, b.item->gpuMesh);
              });

    // 按批绘制（渲染通道已由 FramePipeline 开始）
    const IYalnPipeline* boundPipeline = nullptr;
    const YalnGpuMesh*   boundMesh     = nullptr;

    for (const ResolvedItem& entry : resolved)
    {
        const YalnDrawItem& item = *entry.item;
        const IYalnPipeline& pipeline = *entry.pipeline;

        if (&pipeline != boundPipeline)
        {
            context.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                               *pipeline.handle());

            if (context.frameDescriptorSet != VK_NULL_HANDLE && pipeline.hasDescriptorSets())
            {
                context.commandBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    *pipeline.layout(),
                    0,
                    context.frameDescriptorSet,
                    {});
            }

            boundPipeline = &pipeline;
            boundMesh     = nullptr;
        }

        if (item.gpuMesh != boundMesh)
        {
            context.commandBuffer.bindVertexBuffers(
                0, { item.gpuMesh->vertexBuffer.handle() }, { 0 });
            context.commandBuffer.bindIndexBuffer(
                item.gpuMesh->indexBuffer.handle(), 0, vk::IndexType::eUint32);
            boundMesh = item.gpuMesh;
        }

        if (pipeline.hasPushConstants())
        {
            YalnMaterialPushConstants constants;
            constants.world = item.world;
            constants.baseColor = entry.material != nullptr
                ? entry.material->baseColor()
                : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            const vk::PushConstantRange range = pipeline.config().pushConstant;
            const uint32_t bytes = std::min<uint32_t>(range.size,
                                                      sizeof(YalnMaterialPushConstants));
            if (range.offset == 0 && bytes >= sizeof(glm::mat4))
            {
                context.commandBuffer.pushConstants(
                    *pipeline.layout(),
                    range.stageFlags,
                    range.offset,
                    vk::ArrayProxy<const std::byte>(
                        bytes, asBytes(constants)));
            }
        }

        context.commandBuffer.drawIndexed(item.gpuMesh->indexCount, 1, 0, 0, 0);
    }
}
}  // namespace

void YalnForwardOpaquePass::record(const YalnFrameContext& context)
{
    // 主材质阶段：只画 item.material，honor pipelineOverride（兼容保留）
    recordMaterialPass(context, YalnPassKind::Opaque, MaterialSource::Primary);
}

void YalnMaterialOverlayPass::record(const YalnFrameContext& context)
{
    // 叠加材质阶段：画 item.overlayMaterials[]，完全由材质决定管线
    recordMaterialPass(context, m_pass, MaterialSource::Overlay);
}

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
}

void YalnForwardOpaquePass::record(const YalnFrameContext& context)
{
    // 默认 Forward 的 FramePipeline 负责 beginRenderPass / viewport / endRenderPass；
    // 本 Pass 只在当前激活的渲染通道内录制绘制命令。
    if (context.commandBuffer == VK_NULL_HANDLE || context.scene == nullptr)
    {
        return;
    }

    // ---- 1. 解析绘制项：材质 → 管线 ----
    const uint32_t passBit = passMask();
    std::vector<ResolvedItem> resolved;
    resolved.reserve(context.scene->drawItems().size());

    for (const YalnDrawItem& item : context.scene->drawItems())
    {
        if (item.gpuMesh == nullptr || (item.passMask & passBit) == 0)
        {
            continue;
        }

        const YalnMaterial* material = nullptr;
        YalnMaterialHandle materialHandle = YALN_INVALID_MATERIAL;
        if (context.materials != nullptr)
        {
            material = context.materials->get(item.material);
            materialHandle = item.material;
            if (material == nullptr)
            {
                material = context.materials->get(YALN_DEFAULT_MATERIAL);
                materialHandle = YALN_DEFAULT_MATERIAL;
            }
        }

        if (material != nullptr && (material->passMask() & passBit) == 0)
        {
            continue;
        }

        YalnPipelineHandle pipelineHandle = item.pipelineOverride;
        if (pipelineHandle == YALN_INVALID_PIPELINE)
        {
            pipelineHandle = material != nullptr
                ? material->pipelineForPass(YalnPassKind::Opaque)
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
            continue;
        }

        ResolvedItem entry;
        entry.item           = &item;
        entry.material       = material;
        entry.materialHandle = materialHandle;
        entry.pipeline       = pipeline;
        entry.pipelineHandle = resolvedHandle;
        resolved.push_back(entry);
    }

    // ---- 2. 批处理：pipeline → material → mesh ----
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

    // ---- 3. 按批绘制（渲染通道已由 FramePipeline 开始）----
    const IYalnPipeline* boundPipeline = nullptr;
    const YalnGpuMesh*   boundMesh     = nullptr;

    for (const ResolvedItem& entry : resolved)
    {
        const YalnDrawItem& item = *entry.item;
        const IYalnPipeline& pipeline = *entry.pipeline;

        if (&pipeline != boundPipeline)
        {
            context.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.handle());

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
                    vk::ArrayProxy<const uint8_t>(
                        bytes, reinterpret_cast<const uint8_t*>(&constants)));
            }
        }

        context.commandBuffer.drawIndexed(item.gpuMesh->indexCount, 1, 0, 0, 0);
    }
}

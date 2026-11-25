#ifndef YALN_SCENE_FACTORY_H
#define YALN_SCENE_FACTORY_H

#include <memory>
#include <vector>

#include "yalnmesh.h"
#include "yalnlight.h"
#include "yalnnode.h"

// ============================================================================
//  YalnSceneAsset —— 工厂产出物
//
//  YalnMesh 在场景树中只以裸指针被引用，因此网格的实际所有权必须由 asset
//  持有；只要 asset 存活（且不被替换），场景树中的引用就有效。
//  灯光同样由 asset 收集，调用方再逐个移入 IYalnScene。
// ============================================================================
struct YalnSceneAsset
{
    std::unique_ptr<YalnNode>                 root;
    std::vector<std::unique_ptr<YalnMesh>>    meshes;
    std::vector<std::unique_ptr<YalnLight>>   lights;

    bool empty() const { return root == nullptr; }
};

// ============================================================================
//  YalnSceneFactory —— 场景构建工厂
//
//  把 View 层从“具体场景怎么搭”中解耦：工厂只产出场景树 + 网格 + 灯光，
//  不接触 Vulkan / Renderer。调用方负责 waitIdle、setRoot、addLight、
//  rebuildScene，并持有返回的 asset 保证网格生命周期。
// ============================================================================
class YalnSceneFactory
{
public:
    static YalnSceneAsset createVillage();
    static YalnSceneAsset createEmpty();
};

#endif // YALN_SCENE_FACTORY_H

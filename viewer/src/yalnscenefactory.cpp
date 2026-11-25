#include "../include/yalnscenefactory.h"

#include "yalndrawable.h"
#include "yalnmesh.h"
#include "yalnlight.h"
#include "yalnnode.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <iostream>
#include <string>
#include <utility>

namespace
{
template <typename T, typename... Args>
T* addMesh(YalnSceneAsset& asset, Args&&... args)
{
    auto mesh = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = mesh.get();
    asset.meshes.push_back(std::move(mesh));
    return ptr;
}

template <typename T>
T* addLight(YalnSceneAsset& asset)
{
    auto light = std::make_unique<T>();
    T* ptr = light.get();
    asset.lights.push_back(std::move(light));
    return ptr;
}

std::unique_ptr<YalnLeafNode> makeLeaf(const std::string& name, YalnMesh* mesh,
                                       const Eigen::Vector3f& pos)
{
    auto leaf = std::make_unique<YalnLeafNode>(name);
    auto drawable = std::make_unique<YalnMeshDrawable>(mesh);
    drawable->setPosition(pos);
    leaf->attachDrawable(std::move(drawable));
    return leaf;
}
}  // namespace

YalnSceneAsset YalnSceneFactory::createEmpty()
{
    YalnSceneAsset asset;
    asset.root = std::make_unique<YalnGroupNode>("Root");
    return asset;
}

YalnSceneAsset YalnSceneFactory::createVillage()
{
    YalnSceneAsset asset;
    asset.root = std::make_unique<YalnGroupNode>("Root");
    YalnNode* root = asset.root.get();

    // ---- 网格资源（统一由 asset 持有，场景树只保留裸指针）----
    // 配色：自然村庄色板（低饱和大地色系）
    YalnCube*    groundMesh       = addMesh<YalnCube>(asset, 40.0f, 40.0f, 0.2f, glm::vec3(0.42f, 0.55f, 0.33f));
    YalnCube*    lakeCenterMesh   = addMesh<YalnCube>(asset, 13.0f, 13.0f, 0.03f, glm::vec3(0.30f, 0.48f, 0.62f));
    YalnCube*    lakeSegMesh      = addMesh<YalnCube>(asset, 14.0f, 7.0f, 0.03f, glm::vec3(0.30f, 0.48f, 0.62f));
    YalnCube*    riverMesh        = addMesh<YalnCube>(asset, 5.5f, 2.2f, 0.03f, glm::vec3(0.34f, 0.52f, 0.66f));
    YalnCube*    bridgeMesh       = addMesh<YalnCube>(asset, 1.6f, 2.8f, 0.08f, glm::vec3(0.50f, 0.36f, 0.24f));
    YalnCube*    bridgePostMesh   = addMesh<YalnCube>(asset, 0.14f, 0.14f, 0.5f, glm::vec3(0.42f, 0.30f, 0.20f));
    YalnPyramid* mountainMesh     = addMesh<YalnPyramid>(asset, 5.0f, 5.0f, 3.2f, glm::vec3(0.47f, 0.46f, 0.43f));
    YalnPyramid* snowCapMesh      = addMesh<YalnPyramid>(asset, 2.0f, 2.0f, 1.3f, glm::vec3(0.94f, 0.95f, 0.97f));
    YalnCube*    ppoleMesh        = addMesh<YalnCube>(asset, 0.12f, 0.12f, 2.0f, glm::vec3(0.38f, 0.30f, 0.22f));
    YalnCube*    crossarmMesh     = addMesh<YalnCube>(asset, 0.08f, 0.9f, 0.08f, glm::vec3(0.38f, 0.30f, 0.22f));
    YalnCube*    wireMesh         = addMesh<YalnCube>(asset, 7.0f, 0.025f, 0.025f, glm::vec3(0.22f, 0.22f, 0.24f));
    YalnSphere*  bushMesh         = addMesh<YalnSphere>(asset, 0.22f, 12, glm::vec3(0.30f, 0.44f, 0.24f));
    YalnSphere*  bushMesh2        = addMesh<YalnSphere>(asset, 0.16f, 10, glm::vec3(0.34f, 0.48f, 0.27f));
    YalnSphere*  rockSmallMesh    = addMesh<YalnSphere>(asset, 0.2f, 10, glm::vec3(0.50f, 0.49f, 0.47f));
    YalnCube*    wallFrontMesh    = addMesh<YalnCube>(asset, 2.4f, 0.15f, 1.6f, glm::vec3(0.91f, 0.87f, 0.78f));
    YalnCube*    wallSideMesh     = addMesh<YalnCube>(asset, 0.15f, 1.7f, 1.6f, glm::vec3(0.86f, 0.81f, 0.70f));
    YalnPyramid* roofMesh         = addMesh<YalnPyramid>(asset, 2.8f, 2.5f, 0.8f, glm::vec3(0.68f, 0.38f, 0.30f));
    YalnCube*    flatRoofMesh     = addMesh<YalnCube>(asset, 2.6f, 2.3f, 0.25f, glm::vec3(0.45f, 0.40f, 0.36f));
    YalnCube*    doorMesh         = addMesh<YalnCube>(asset, 0.7f, 0.10f, 1.3f, glm::vec3(0.40f, 0.28f, 0.18f));
    YalnCube*    windowMeshF      = addMesh<YalnCube>(asset, 0.6f, 0.10f, 0.5f, glm::vec3(0.58f, 0.72f, 0.82f));
    YalnCube*    windowMeshS      = addMesh<YalnCube>(asset, 0.10f, 0.6f, 0.5f, glm::vec3(0.58f, 0.72f, 0.82f));
    YalnCube*    poleMesh         = addMesh<YalnCube>(asset, 0.25f, 0.25f, 2.2f, glm::vec3(0.48f, 0.48f, 0.50f));
    YalnCube*    bladeMesh        = addMesh<YalnCube>(asset, 2.2f, 0.2f, 0.3f, glm::vec3(0.93f, 0.92f, 0.90f));
    YalnCube*    bladeMesh2       = addMesh<YalnCube>(asset, 0.3f, 0.2f, 2.2f, glm::vec3(0.72f, 0.38f, 0.32f));
    YalnCube*    markerMesh       = addMesh<YalnCube>(asset, 0.06f, 1.2f, 0.8f, glm::vec3(0.93f, 0.92f, 0.90f));
    YalnCube*    roadMeshNS       = addMesh<YalnCube>(asset, 1.6f, 32.0f, 0.04f, glm::vec3(0.45f, 0.45f, 0.47f));
    YalnCube*    roadMeshEW       = addMesh<YalnCube>(asset, 38.0f, 1.6f, 0.04f, glm::vec3(0.48f, 0.48f, 0.50f));
    YalnCube*    trunkMesh        = addMesh<YalnCube>(asset, 0.18f, 0.18f, 0.8f, glm::vec3(0.42f, 0.33f, 0.24f));
    YalnSphere*  canopyMeshA      = addMesh<YalnSphere>(asset, 0.52f, 20, glm::vec3(0.34f, 0.47f, 0.28f));
    YalnSphere*  canopyMeshB      = addMesh<YalnSphere>(asset, 0.38f, 16, glm::vec3(0.40f, 0.53f, 0.32f));
    YalnSphere*  rockMesh         = addMesh<YalnSphere>(asset, 0.45f, 24, glm::vec3(0.52f, 0.51f, 0.49f));
    YalnSphere*  lampBulbMesh     = addMesh<YalnSphere>(asset, 0.12f, 12, glm::vec3(1.0f, 0.92f, 0.62f));

    // ---- 子树工厂：树 = 树干 + 双层球冠 ----
    auto makeTree = [trunkMesh, canopyMeshA, canopyMeshB](const std::string& name, float scale) {
        auto tree = std::make_unique<YalnGroupNode>(name);
        tree->setScale(Eigen::Vector3f::Constant(scale));

        auto trunk = makeLeaf(name + ".trunk", trunkMesh, Eigen::Vector3f(0.0f, 0.0f, 0.4f));
        auto canopyA = makeLeaf(name + ".canopyA", canopyMeshA, Eigen::Vector3f(0.0f, 0.0f, 1.15f));
        auto canopyB = makeLeaf(name + ".canopyB", canopyMeshB, Eigen::Vector3f(0.12f, -0.08f, 1.5f));

        tree->addChild(std::move(trunk));
        tree->addChild(std::move(canopyA));
        tree->addChild(std::move(canopyB));
        return tree;
    };

    // ---- 子树工厂：房子 = 四面墙 + 门 + 四扇窗 + 屋顶 ----
    auto makeHouse = [wallFrontMesh, wallSideMesh, doorMesh, windowMeshF, windowMeshS,
                      roofMesh, flatRoofMesh](const std::string& name, bool withFlatRoofOption) {
        auto house = std::make_unique<YalnGroupNode>(name);
        constexpr float kWallH = 1.6f;

        auto addLeaf = [&house](const std::string& leafName, YalnMesh* mesh,
                                const Eigen::Vector3f& pos) {
            house->addChild(makeLeaf(leafName, mesh, pos));
        };

        addLeaf(name + ".wallF", wallFrontMesh, Eigen::Vector3f( 0.0f,  -0.925f, kWallH * 0.5f));
        addLeaf(name + ".wallB", wallFrontMesh, Eigen::Vector3f( 0.0f,   0.925f, kWallH * 0.5f));
        addLeaf(name + ".wallL", wallSideMesh,  Eigen::Vector3f(-1.125f,  0.0f,  kWallH * 0.5f));
        addLeaf(name + ".wallR", wallSideMesh,  Eigen::Vector3f( 1.125f,  0.0f,  kWallH * 0.5f));
        addLeaf(name + ".door",  doorMesh,    Eigen::Vector3f( 0.55f, -0.98f, 0.65f));
        addLeaf(name + ".winF",  windowMeshF, Eigen::Vector3f(-0.55f, -0.98f, 1.0f));
        addLeaf(name + ".winB",  windowMeshF, Eigen::Vector3f(-0.55f,  0.98f, 1.0f));
        addLeaf(name + ".winL",  windowMeshS, Eigen::Vector3f(-1.16f,  0.0f,  1.0f));
        addLeaf(name + ".winR",  windowMeshS, Eigen::Vector3f( 1.16f,  0.0f,  1.0f));

        std::unique_ptr<YalnNode> roof;
        if (withFlatRoofOption)
        {
            auto roofSwitch = std::make_unique<YalnSwitchNode>(name + ".roofStyle");
            roofSwitch->addChild(makeLeaf(name + ".roofPyramid", roofMesh, Eigen::Vector3f(0.0f, 0.0f, 2.0f)));
            roofSwitch->addChild(makeLeaf(name + ".roofFlat", flatRoofMesh, Eigen::Vector3f(0.0f, 0.0f, 1.725f)));
            roofSwitch->setActiveChild(name + ".roofPyramid");
            roof = std::move(roofSwitch);
        }
        else
        {
            roof = makeLeaf(name + ".roof", roofMesh, Eigen::Vector3f(0.0f, 0.0f, 2.0f));
        }
        house->addChild(std::move(roof));
        return house;
    };

    // ---- 地面 ----
    root->addChild(makeLeaf("ground", groundMesh, Eigen::Vector3f(0.0f, 0.0f, -0.1f)));

    // ---- 十字路 ----
    root->addChild(makeLeaf("road.ns", roadMeshNS, Eigen::Vector3f(-2.0f, -3.5f, 0.025f)));
    root->addChild(makeLeaf("road.ew", roadMeshEW, Eigen::Vector3f(0.0f, 1.0f, 0.025f)));

    // ---- 河流 + 湖 + 桥 ----
    {
        const std::pair<Eigen::Vector3f, float> riverSpans[] = {
            { Eigen::Vector3f(-16.5f, -8.0f, 0.02f), glm::radians(  8.0f) },
            { Eigen::Vector3f(-11.5f, -8.6f, 0.02f), glm::radians(-10.0f) },
            { Eigen::Vector3f( -6.5f, -8.2f, 0.02f), glm::radians(  6.0f) },
            { Eigen::Vector3f( -2.0f, -8.8f, 0.02f), glm::radians( -6.0f) },
            { Eigen::Vector3f(  1.5f, -9.0f, 0.02f), glm::radians(  0.0f) },
        };
        for (size_t i = 0; i < std::size(riverSpans); i++)
        {
            auto seg = makeLeaf("river" + std::to_string(i + 1), riverMesh, riverSpans[i].first);
            seg->setEulerRotation(Eigen::Vector3f(0.0f, 0.0f, riverSpans[i].second));
            root->addChild(std::move(seg));
        }

        const Eigen::Vector3f lakeCenter(8.0f, -9.0f, 0.02f);
        auto lakeBody = std::make_unique<YalnGroupNode>("lake");
        lakeBody->addChild(makeLeaf("lake.center", lakeCenterMesh, lakeCenter));
        for (int i = 0; i < 6; i++)
        {
            auto seg = makeLeaf("lake.seg" + std::to_string(i + 1), lakeSegMesh, lakeCenter);
            seg->setEulerRotation(Eigen::Vector3f(0.0f, 0.0f, glm::radians(30.0f * i)));
            lakeBody->addChild(std::move(seg));
        }
        root->addChild(std::move(lakeBody));

        auto bridge = std::make_unique<YalnGroupNode>("bridge");
        bridge->setPosition(Eigen::Vector3f(-2.0f, -8.8f, 0.06f));
        bridge->addChild(makeLeaf("bridge.deck", bridgeMesh, Eigen::Vector3f(0.0f, 0.0f, 0.0f)));

        const Eigen::Vector3f postOffsets[] = {
            Eigen::Vector3f(-0.70f, -1.30f, 0.19f), Eigen::Vector3f(0.70f, -1.30f, 0.19f),
            Eigen::Vector3f(-0.70f,  1.30f, 0.19f), Eigen::Vector3f(0.70f,  1.30f, 0.19f),
        };
        for (size_t i = 0; i < std::size(postOffsets); i++)
        {
            bridge->addChild(makeLeaf("bridge.post" + std::to_string(i + 1), bridgePostMesh, postOffsets[i]));
        }
        root->addChild(std::move(bridge));
    }

    // ---- 十五栋房子 ----
    struct HouseSpec { Eigen::Vector3f position; float rotationZ; float scale; };
    const HouseSpec houses[] = {
        { Eigen::Vector3f( -5.0f,  -4.0f, 0.0f), glm::radians( 15.0f), 1.00f },
        { Eigen::Vector3f( -8.0f,  -1.5f, 0.0f), glm::radians(-20.0f), 0.90f },
        { Eigen::Vector3f( -5.5f,   4.0f, 0.0f), glm::radians( 10.0f), 1.10f },
        { Eigen::Vector3f(  1.0f,  -3.5f, 0.0f), glm::radians(-30.0f), 0.85f },
        { Eigen::Vector3f(  2.0f,   3.5f, 0.0f), glm::radians(  5.0f), 0.95f },
        { Eigen::Vector3f(  5.5f,   2.8f, 0.0f), glm::radians(-25.0f), 0.80f },
        { Eigen::Vector3f(  8.5f,   4.5f, 0.0f), glm::radians( 15.0f), 1.05f },
        { Eigen::Vector3f(  4.5f,   7.5f, 0.0f), glm::radians( 30.0f), 0.90f },
        { Eigen::Vector3f(-10.0f,   6.5f, 0.0f), glm::radians(-10.0f), 1.00f },
        { Eigen::Vector3f( 12.0f,   7.0f, 0.0f), glm::radians( 20.0f), 0.95f },
        { Eigen::Vector3f(  5.5f,  -1.0f, 0.0f), glm::radians( 40.0f), 1.10f },
        { Eigen::Vector3f( 13.5f,  -1.5f, 0.0f), glm::radians(-15.0f), 0.90f },
        { Eigen::Vector3f( 16.5f,   4.0f, 0.0f), glm::radians( 10.0f), 0.85f },
        { Eigen::Vector3f(-14.0f,  -3.0f, 0.0f), glm::radians(-40.0f), 1.00f },
        { Eigen::Vector3f(-16.0f,   3.0f, 0.0f), glm::radians(-30.0f), 0.90f },
    };
    int houseIndex = 1;
    for (const HouseSpec& spec : houses)
    {
        auto house = makeHouse("house" + std::to_string(houseIndex), houseIndex == 2);
        house->setPosition(spec.position);
        house->setEulerRotation(Eigen::Vector3f(0.0f, 0.0f, spec.rotationZ));
        house->setScale(Eigen::Vector3f::Constant(spec.scale));
        root->addChild(std::move(house));
        houseIndex++;
    }

    // ---- 山脉 ----
    auto makeMountain = [mountainMesh, snowCapMesh](const std::string& name,
                                                    const Eigen::Vector3f& pos, float scale) {
        auto m = std::make_unique<YalnGroupNode>(name);
        m->setPosition(pos);
        m->setScale(Eigen::Vector3f::Constant(scale));
        m->addChild(makeLeaf(name + ".body", mountainMesh, Eigen::Vector3f(0.0f, 0.0f, 1.6f)));
        m->addChild(makeLeaf(name + ".snow", snowCapMesh, Eigen::Vector3f(0.0f, 0.0f, 2.55f)));
        return m;
    };
    {
        const std::pair<Eigen::Vector3f, float> mountains[] = {
            { Eigen::Vector3f(-10.0f, 17.5f, 0.0f), 1.60f },
            { Eigen::Vector3f(  2.0f, 18.5f, 0.0f), 2.00f },
            { Eigen::Vector3f( 13.0f, 17.0f, 0.0f), 1.50f },
            { Eigen::Vector3f(-18.0f, 12.0f, 0.0f), 1.20f },
            { Eigen::Vector3f( 18.5f, 11.0f, 0.0f), 1.30f },
            { Eigen::Vector3f( 18.0f, 18.0f, 0.0f), 1.00f },
        };
        for (size_t i = 0; i < std::size(mountains); i++)
        {
            root->addChild(makeMountain("mountain" + std::to_string(i + 1),
                                        mountains[i].first, mountains[i].second));
        }
    }

    // ---- 树 ----
    const std::pair<Eigen::Vector3f, float> trees[] = {
        { Eigen::Vector3f(-13.0f,  -6.5f, 0.0f), 1.1f },
        { Eigen::Vector3f(-18.0f,   0.0f, 0.0f), 1.0f },
        { Eigen::Vector3f(-18.0f,   8.0f, 0.0f), 1.0f },
        { Eigen::Vector3f( -9.0f,  10.0f, 0.0f), 0.9f },
        { Eigen::Vector3f( -1.0f,  11.0f, 0.0f), 1.0f },
        { Eigen::Vector3f(  7.0f,  11.0f, 0.0f), 1.1f },
        { Eigen::Vector3f( 15.0f,  11.5f, 0.0f), 0.9f },
        { Eigen::Vector3f( 18.0f,  -1.0f, 0.0f), 0.9f },
        { Eigen::Vector3f( 11.0f, -17.0f, 0.0f), 1.0f },
        { Eigen::Vector3f( -6.0f, -13.0f, 0.0f), 1.2f },
        { Eigen::Vector3f(-12.0f, -12.0f, 0.0f), 1.0f },
        { Eigen::Vector3f( 17.0f, -14.0f, 0.0f), 0.8f },
    };
    for (size_t i = 0; i < std::size(trees); i++)
    {
        auto tree = makeTree("tree" + std::to_string(i + 1), trees[i].second);
        tree->setPosition(trees[i].first);
        root->addChild(std::move(tree));
    }

    // ---- 灌木 ----
    const std::pair<Eigen::Vector3f, float> bushes[] = {
        { Eigen::Vector3f( -3.5f,  2.2f, 0.0f), 1.0f },
        { Eigen::Vector3f(  3.2f,  2.6f, 0.0f), 0.8f },
        { Eigen::Vector3f( -6.5f, -2.0f, 0.0f), 1.0f },
        { Eigen::Vector3f(  9.5f,  2.8f, 0.0f), 1.0f },
        { Eigen::Vector3f( 15.5f, -4.5f, 0.0f), 1.0f },
        { Eigen::Vector3f(  0.5f, -6.0f, 0.0f), 0.7f },
        { Eigen::Vector3f(-16.0f, -5.5f, 0.0f), 0.8f },
        { Eigen::Vector3f( -3.0f,-11.0f, 0.0f), 0.9f },
        { Eigen::Vector3f(  6.8f,  7.8f, 0.0f), 1.0f },
        { Eigen::Vector3f(-11.0f,  3.5f, 0.0f), 0.8f },
    };
    for (size_t i = 0; i < std::size(bushes); i++)
    {
        auto bush = makeLeaf("bush" + std::to_string(i + 1),
                             (i % 2) ? bushMesh2 : bushMesh,
                             Eigen::Vector3f(bushes[i].first.x(), bushes[i].first.y(),
                                             0.22f * bushes[i].second));
        bush->setScale(Eigen::Vector3f::Constant(bushes[i].second));
        root->addChild(std::move(bush));
    }

    // ---- 岩石群 ----
    {
        const Eigen::Vector3f rockSpots[] = {
            Eigen::Vector3f( -7.5f,  -6.5f, 0.0f), Eigen::Vector3f( -1.0f, -12.0f, 0.0f),
            Eigen::Vector3f( 18.0f, -16.0f, 0.0f), Eigen::Vector3f( 17.0f,  -8.5f, 0.0f),
            Eigen::Vector3f(-14.0f, -14.0f, 0.0f),
        };
        const float rockScales[] = { 1.0f, 0.8f, 0.9f, 1.1f, 0.9f };
        for (size_t i = 0; i < std::size(rockSpots); i++)
        {
            auto rock = makeLeaf("rock." + std::to_string(i + 1), rockSmallMesh,
                                 Eigen::Vector3f(rockSpots[i].x(), rockSpots[i].y(),
                                                 0.2f * rockScales[i]));
            rock->setScale(Eigen::Vector3f::Constant(rockScales[i]));
            root->addChild(std::move(rock));
        }
    }

    // ---- 电力设施 ----
    {
        const float wireY = 2.4f;
        const float poleXs[] = { -14.0f, -7.0f, 0.0f, 7.0f, 14.0f };
        for (size_t i = 0; i < std::size(poleXs); i++)
        {
            const std::string base = "pole" + std::to_string(i + 1);
            auto pole = std::make_unique<YalnGroupNode>(base);
            pole->setPosition(Eigen::Vector3f(poleXs[i], wireY, 0.0f));
            pole->addChild(makeLeaf(base + ".post", ppoleMesh, Eigen::Vector3f(0.0f, 0.0f, 1.0f)));
            pole->addChild(makeLeaf(base + ".arm", crossarmMesh, Eigen::Vector3f(0.0f, 0.0f, 1.8f)));
            root->addChild(std::move(pole));
        }
        for (size_t i = 0; i + 1 < std::size(poleXs); i++)
        {
            const float cx = (poleXs[i] + poleXs[i + 1]) * 0.5f;
            for (int s = 0; s < 2; s++)
            {
                auto wire = makeLeaf("wire" + std::to_string(i + 1) + "_" + std::to_string(s + 1),
                                     wireMesh,
                                     Eigen::Vector3f(cx, wireY + (s == 0 ? -0.32f : 0.32f), 1.78f));
                root->addChild(std::move(wire));
            }
        }
    }

    // ---- 风车 ----
    {
        auto windmill = std::make_unique<YalnGroupNode>("windmill");
        auto windmillPos = std::make_unique<YalnTranslateNode>("windmill.pos");
        windmillPos->setOffset(Eigen::Vector3f(-5.5f, 9.5f, 0.0f));
        windmillPos->addChild(makeLeaf("windmill.base", poleMesh, Eigen::Vector3f(0.0f, 0.0f, 1.1f)));

        auto rotor = std::make_unique<YalnSpinNode>("windmill.rotor");
        rotor->setPosition(Eigen::Vector3f(0.0f, 0.0f, 2.2f));
        rotor->setAxis(Eigen::Vector3f(0.0f, 1.0f, 0.0f));
        rotor->setSpeed(glm::radians(120.0f));
        rotor->addChild(makeLeaf("windmill.bladeA", bladeMesh, Eigen::Vector3f(0.0f, 0.0f, 0.0f)));
        rotor->addChild(makeLeaf("windmill.bladeB", bladeMesh2, Eigen::Vector3f(0.0f, 0.0f, 0.0f)));

        windmillPos->addChild(std::move(rotor));
        windmill->addChild(std::move(windmillPos));
        root->addChild(std::move(windmill));
    }

    // ---- 公告板 ----
    {
        auto marker = std::make_unique<YalnBillboardNode>("marker");
        marker->setPosition(Eigen::Vector3f(-5.5f, 9.5f, 3.4f));
        marker->attachDrawable(std::make_unique<YalnMeshDrawable>(markerMesh));
        root->addChild(std::move(marker));
    }

    // ---- 变换链岩石 ----
    {
        auto rock = std::make_unique<YalnTranslateNode>("rock");
        rock->setOffset(Eigen::Vector3f(14.5f, 10.0f, 0.0f));
        auto rockTilt = std::make_unique<YalnRotateNode>("rock.tilt");
        rockTilt->setAxisAngle(Eigen::Vector3f(0.0f, 1.0f, 0.0f), glm::radians(-18.0f));
        auto rockSize = std::make_unique<YalnScaleNode>("rock.size");
        rockSize->setScaleFactor(Eigen::Vector3f(1.3f, 1.0f, 0.8f));
        rockSize->addChild(makeLeaf("rock.body", rockMesh, Eigen::Vector3f(0.0f, 0.0f, 0.35f)));
        rockTilt->addChild(std::move(rockSize));
        rock->addChild(std::move(rockTilt));
        root->addChild(std::move(rock));
    }

    // ---- 灯光 ----
    {
        YalnAmbientLight* ambient = addLight<YalnAmbientLight>(asset);
        ambient->setColor(glm::vec3(0.58f, 0.65f, 0.78f));
        ambient->setIntensity(0.55f);

        YalnDirectionalLight* sun = addLight<YalnDirectionalLight>(asset);
        sun->setDirection(glm::normalize(glm::vec3(-0.45f, -0.35f, -0.9f)));
        sun->setColor(glm::vec3(1.00f, 0.96f, 0.88f));
        sun->setIntensity(0.9f);

        const glm::vec3 lampSpots[] = {
            glm::vec3(0.0f, 2.4f, 1.62f),
            glm::vec3(7.0f, 2.4f, 1.62f),
        };
        for (int i = 0; i < 2; i++)
        {
            root->addChild(makeLeaf("lamp." + std::to_string(i + 1), lampBulbMesh,
                                    Eigen::Vector3f(lampSpots[i].x, lampSpots[i].y, lampSpots[i].z)));

            YalnPointLight* lamp = addLight<YalnPointLight>(asset);
            lamp->setPosition(lampSpots[i]);
            lamp->setColor(glm::vec3(1.0f, 0.85f, 0.55f));
            lamp->setIntensity(1.3f);
            lamp->setRange(8.0f);
        }

        YalnSpotLight* searchLight = addLight<YalnSpotLight>(asset);
        searchLight->setPosition(glm::vec3(-5.5f, 9.5f, 2.6f));
        searchLight->setDirection(glm::normalize(glm::vec3(0.15f, 0.25f, -1.0f)));
        searchLight->setColor(glm::vec3(1.0f, 0.95f, 0.85f));
        searchLight->setIntensity(1.6f);
        searchLight->setRange(14.0f);
        searchLight->setAngles(18.0f, 30.0f);
    }

    std::cout << "Scene factory: nodes=" << root->countNodes()
              << ", drawables=" << root->countDrawables()
              << ", meshes=" << asset.meshes.size()
              << ", lights=" << asset.lights.size() << std::endl;

    return asset;
}

project(yaln_core VERSION 1.0.0 LANGUAGES CXX)

set(YALN_CORE_SRCS
    # ---- 门面 ----
    ./src/yalnview.cpp

    # ---- 基础设施 ----
    ./src/yalnvulkaninstance.cpp   # 实例 / 校验层 / 调试信使
    ./src/yalndevice.cpp           # 物理设备 / 逻辑设备 / 队列 / 显存类型

    # ---- 渲染资源 ----
    ./src/yalnswapchain.cpp        # 交换链 / 图像视图
    ./src/yalnrenderpass.cpp       # 渲染通道 / 帧缓冲
    ./src/yalngraphicspipeline.cpp # 着色器 / 描述符集布局 / 管线

    # ---- 数据与渲染循环 ----
    ./src/yalnbuffer.cpp           # 缓冲 + 显存
    ./src/yalnscene.cpp            # 场景（网格容器 + GPU 缓冲）
    ./src/yalnrenderer.cpp         # 命令缓冲 / 同步 / UBO / 描述符 / drawFrame

    # ---- 其它 ----
    ./src/yalnvertex.cpp
    ./src/event.cpp
    ./src/yalncamera.cpp
    ./src/yalnperspectivecamera.cpp
    ./src/yalnmesh.cpp
)
set(YALN_CORE_HDRS
    ./include/yalnview.h

    ./include/yalnvulkaninstance.h
    ./include/yalnsurface.h
    ./include/yalndevice.h

    ./include/yalnswapchain.h
    ./include/yalnrenderpass.h
    ./include/yalngraphicspipeline.h

    ./include/yalnbuffer.h
    ./include/yalnscene.h
    ./include/yalnrenderer.h

    ./include/yalnvertex.h
    ./include/event.h
    ./include/yalncamera.h
    ./include/yalnperspectivecamera.h
    ./include/yalnmesh.h
)

add_library(${PROJECT_NAME} SHARED ${YALN_CORE_SRCS} ${YALN_CORE_HDRS})
configure_vulkan(${PROJECT_NAME})

# Windows: 定义 MYLIB_EXPORTS
# Linux/Mac: 定义 MYLIB_EXPORTS 并设置可见性
target_compile_definitions(${PROJECT_NAME} PRIVATE YALN_CORE_EXPORTS)

# 对于 Linux/Mac，设置默认隐藏可见性
if(UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
#################################################################################
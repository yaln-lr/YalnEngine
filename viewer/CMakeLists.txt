cmake_minimum_required(VERSION 3.10)
project(qtviewer VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 Qt 自动元对象编译
set(CMAKE_AUTOMOC ON)

# 查找 Qt6
set(Qt6_DIR "D:/QT6/6.11.0/mingw_64/lib/cmake/Qt6")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# 查找 Vulkan
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    set(Vulkan_INCLUDE_DIRS "D:/VulkanSDK/1.4.341.1/Include")
endif()

# 查找 GLM
find_package(glm QUIET)
if(NOT glm_FOUND)
    set(GLM_INCLUDE_DIRS "E:/Vulkan/glm-master")
endif()

# core 模块已在父级 CMakeLists.txt 中添加

# 源文件列表
set(VIEWER_SOURCES
    src/main.cpp
    src/qtviewer.cpp
)

set(VIEWER_HEADERS
    include/qtviewer.h
)

# 创建可执行文件
add_executable(${PROJECT_NAME} 
    ${VIEWER_SOURCES}
    ${VIEWER_HEADERS}
)

# 头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/viewer/include
    ${Vulkan_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

# Vulkan 配置
configure_vulkan(${PROJECT_NAME})

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    yaln_core
)

# 复制着色器到构建目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shader/shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

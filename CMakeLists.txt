cmake_minimum_required(VERSION 3.10)

project(VulkanMaster VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# ======================== 公共依赖 ========================

# Vulkan
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    set(Vulkan_INCLUDE_DIRS "D:/VulkanSDK/1.4.341.1/Include")
endif()

# GLM
find_package(glm QUIET)
if(NOT glm_FOUND)
    set(GLM_INCLUDE_DIRS "E:/Vulkan/glm-master")
endif()

# Qt6
set(Qt6_DIR "D:/QT6/6.11.0/mingw_64/lib/cmake/Qt6")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)



set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

# ======================== 公共函数 ========================

# 为目标添加Vulkan平台配置（头文件、链接库）
function(configure_vulkan TARGET)
    target_include_directories(${TARGET} PRIVATE ${Vulkan_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
    if(WIN32)
        target_compile_definitions(${TARGET} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
        target_link_libraries(${TARGET} PRIVATE -lvulkan-1)
    elseif(APPLE)
        target_compile_definitions(${TARGET} PRIVATE VK_USE_PLATFORM_MACOS_MVK)
        find_library(VULKAN_LIB vulkan HINTS /opt/homebrew/lib ~/Library/Frameworks)
        target_link_libraries(${TARGET} PRIVATE ${VULKAN_LIB}
            "-framework Cocoa" "-framework QuartzCore" "-framework IOKit")
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(${TARGET} PRIVATE VK_USE_PLATFORM_XCB_KHR)
        find_package(XCB REQUIRED COMPONENTS XCB)
        target_include_directories(${TARGET} PRIVATE ${XCB_INCLUDE_DIRS})
        target_link_libraries(${TARGET} PRIVATE vulkan ${XCB_LIBRARIES})
    endif()
endfunction()

# 安装第三方库
install(DIRECTORY ${THIRDPARTY_DIR}/bin DESTINATION /)
# ======================== 子项目 ========================
# 定义选项，默认为 ON
option(BUILD_EXAMPLES "Build example programs" OFF)

if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

add_subdirectory(shader)
add_subdirectory(core)
add_subdirectory(viewer)
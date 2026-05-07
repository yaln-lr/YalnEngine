cmake_minimum_required(VERSION 3.20)

# GLFW Viewer 测试工程
project(glfwviewer VERSION 1.0.0 LANGUAGES CXX)

# 源文件
set(YALN_GLFWVIEWER_SRCS
    src/main.cpp
    src/glfwviewer.cpp
)
# 源文件
set(YALN_GLFWVIEWER_HDRS
    src/main.cpp
    src/glfwviewer.cpp
)

# 头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/core/include
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${YALN_GLFWVIEWER_SRCS} ${YALN_GLFWVIEWER_SRCS})
target_link_libraries(glfwviewer PRIVATE yaln_core)
configure_vulkan(${PROJECT_NAME})
# GLFW
if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE F:/Vulkan/glfw-3.4.bin.WIN64/include)
    target_link_libraries(${PROJECT_NAME} PRIVATE F:/Vulkan/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3dll.lib)
elseif(APPLE)
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    endif()
elseif(UNIX)
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    endif()
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

cmake_minimum_required(VERSION 3.10)
#ind_package(Vulkan REQUIRED)
# 共享着色器目

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

set(STAGE_VERT "-fshader-stage=vert")
set(STAGE_FRAG "-fshader-stage=frag")

set(SPV_DIR ${CMAKE_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${SPV_DIR})

set(GRAPHICS_VERT_SHADER ${SHADER_DIR}/graphics.vert.glsl)
set(GRAPHICS_FRAG_SHADER ${SHADER_DIR}/graphics.frag.glsl)
set(GRAPHICS_SPIRV_VERT ${SPV_DIR}/graphics.vert.spv)
set(GRAPHICS_SPIRV_FRAG ${SPV_DIR}/graphics.frag.spv)



find_program(GLSLC_EXECUTABLE glslc)

# 编译共享着色器（输出到 build/shaders）
if(GLSLC_EXECUTABLE)
    add_custom_command(
            OUTPUT ${GRAPHICS_SPIRV_VERT}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${STAGE_VERT} ${GRAPHICS_VERT_SHADER} -o ${GRAPHICS_SPIRV_VERT}
            COMMENT "Compiling graphics.vert.glsl to graphics.vert.spv"
            DEPENDS ${GRAPHICS_VERT_SHADER}
    )

    add_custom_command(
            OUTPUT ${GRAPHICS_SPIRV_FRAG}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${STAGE_FRAG} ${GRAPHICS_FRAG_SHADER} -o ${GRAPHICS_SPIRV_FRAG}
            COMMENT "Compiling graphics.frag.glsl to graphics.frag.spv"
            DEPENDS ${GRAPHICS_FRAG_SHADER}
    )

    add_custom_target(CompileShaders ALL
            DEPENDS ${GRAPHICS_SPIRV_VERT} ${GRAPHICS_SPIRV_FRAG}
    )

    add_custom_command(OUTPUT ${SPV_DIR}/shader.vert.spv
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_DIR}/shader.vert -o ${SPV_DIR}/shader.vert.spv
        DEPENDS ${SHADER_DIR}/shader.vert COMMENT "Compiling vertex shader")

    add_custom_command(OUTPUT ${SPV_DIR}/shader.frag.spv
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_DIR}/shader.frag -o ${SPV_DIR}/shader.frag.spv
        DEPENDS ${SHADER_DIR}/shader.frag COMMENT "Compiling fragment shader")

    add_custom_target(compile_shaders ALL
        DEPENDS ${SPV_DIR}/shader.vert.spv ${SPV_DIR}/shader.frag.spv)
        
else()
    message(WARNING "glslc not found, shader compilation skipped")
endif()

# 安装着色器
install(DIRECTORY ${SPV_DIR}/ DESTINATION shaders/)


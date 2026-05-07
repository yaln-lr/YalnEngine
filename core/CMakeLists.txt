project(yaln_core VERSION 1.0.0 LANGUAGES CXX)

set(YALN_CORE_SRCS
    ./src/yalnview.cpp
    ./src/yalnvertex.cpp
    ./src/event.cpp
    ./src/yalncamera.cpp
    ./src/yalnperspectivecamera.cpp
    #./src/yalngpubuffer.cpp
    #./src/yalnmodel.cpp
    #./src/yalnpipelinemanager.cpp
    ./src/yalnmesh.cpp

    #./hpp/yalnmeshdata.hpp
    #./hpp/yalnvertex.hpp
    #./hpp/vertexattribute.hpp
   
)
set(YALN_CORE_HDRS
    ./include/yalnview.h
    ./include/yalnvertex.h
    ./include/event.h
    ./include/yalncamera.h
    ./include/yalnperspectivecamera.h
    #./include/yalngpubuffer.h
    #./include/yalnmodel.h
    #./include/yalnpipelinemanager.h
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
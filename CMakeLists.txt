cmake_minimum_required(VERSION 3.26.0)
project(YARenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(SRC_FILES 
    src/main.cpp

    src/Application.h
    src/Application.cpp

    src/core/Window.h
    src/core/Window.cpp

    src/core/Log.h
    src/core/Log.cpp

    src/core/Timer.h
    src/core/Timer.cpp

    src/core/UI.h
    src/core/UI.cpp

    src/core/MathHelper.h
    src/core/MathHelper.cpp

    src/event/Event.h
    src/event/ApplicationEvent.h
    src/event/KeyEvent.h
    src/event/MouseEvent.h

    src/dx/dx.h

    src/dx/CommandQueue.h
    src/dx/CommandQueue.cpp

    src/dx/Descriptor.h

    src/dx/DescriptorHeap.h
    src/dx/DescriptorHeap.cpp

    src/dx/DxContext.h
    src/dx/DxContext.cpp

    src/dx/Texture.h
    src/dx/Texture.cpp
    
    src/dx/UploadBuffer.h
    
    src/dx/Utils.h
    src/dx/Utils.cpp

    src/rendering/Camera.h
    src/rendering/Camera.cpp

    src/rendering/CascadedShadowmap.h
    src/rendering/CascadedShadowmap.cpp

    src/rendering/EnvironmentMap.h
    src/rendering/EnvironmentMap.cpp

    src/rendering/FrameResource.h

    src/rendering/Light.h

    src/rendering/Material.h

    src/rendering/Mesh.h
    src/rendering/Mesh.cpp

    src/rendering/PipelineStateManager.h
    src/rendering/PipelineStateManager.cpp

    src/rendering/Renderer.h
    src/rendering/Renderer.cpp

    src/rendering/RenderingSettings.h

    src/rendering/RenderingUtils.h
    src/rendering/RenderingUtils.cpp

    src/rendering/SSAO.h
    src/rendering/SSAO.cpp

    src/rendering/TAA.h
    src/rendering/TAA.cpp

    src/rendering/VXGI.h
    src/rendering/VXGI.cpp

    src/rendering/PostProcessing.h
    src/rendering/PostProcessing.cpp

    src/rendering/PipelineStates.h
    src/rendering/PipelineStates.cpp

    src/asset/Image.h
    src/asset/Image.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE 
    src 
    external/directx/include
)

# assimp
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_compile_definitions(ASSIMP_USE_STB_IMAGE_STATIC)
add_compile_definitions(ASSIMP_BUILD_NO_PBRT_EXPORTER)

add_subdirectory(external/assimp)
target_include_directories(${PROJECT_NAME}
    PRIVATE external/assimp/include
    PRIVATE external/assimp/contrib
)

# imgui
add_library(imgui
    external/imgui/imconfig.h
    external/imgui/imgui.h
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_internal.h
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imstb_rectpack.h
    external/imgui/imstb_textedit.h
    external/imgui/imstb_truetype.h
    external/imgui/backends/imgui_impl_dx12.cpp
    external/imgui/backends/imgui_impl_dx12.h
    external/imgui/backends/imgui_impl_win32.cpp
    external/imgui/backends/imgui_impl_win32.h
)
target_include_directories(imgui PUBLIC
    external/imgui
    external/imgui/backends
)

# spdlog
target_include_directories(${PROJECT_NAME} PRIVATE external/spdlog/include)

# linking libraries
target_link_libraries(${PROJECT_NAME} assimp imgui dxcompiler.lib d3d12.lib d3dcompiler.lib dxgi.lib dxguid.lib)

# setup precompiled headers
target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
    "src/pch.h"
)
cmake_minimum_required(VERSION 3.26.0)
project(YARenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(${PROJECT_NAME} src/main.cpp)

# assimp
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)

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

target_link_libraries(${PROJECT_NAME} assimp imgui)
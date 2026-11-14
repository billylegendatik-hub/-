cmake_minimum_required(VERSION 3.20)
project(TheWall LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.8)
FetchContent_MakeAvailable(imgui)
add_executable(TheWall WIN32
    main.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(TheWall PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(TheWall PRIVATE d3d11 dxgi d3dcompiler comdlg32 shell32)
target_compile_definitions(TheWall PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(TheWall PRIVATE /W4 /permissive-)
endif()

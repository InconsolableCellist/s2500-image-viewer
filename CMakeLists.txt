cmake_minimum_required(VERSION 3.20)
project(s2500_image_viewer)

set(CMAKE_CXX_STANDARD 14)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
add_library("glad" "glad/src/glad.c")

include_directories("glad/include")
include_directories(${SDL2_INCLUDE_DIRS})
add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GLAD)

set(sources
    imgui/imconfig.h
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui.h
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui/imgui_impl_sdl.h
    imgui/imgui_impl_sdl.cpp
    imgui/imgui_impl_opengl3.cpp
    imgui/imgui_impl_opengl3.h
    imgui/imgui_impl_opengl3_loader.h
    main.cpp)

add_executable(${CMAKE_PROJECT_NAME} ${sources})

target_link_libraries(${CMAKE_PROJECT_NAME}
    "glad"
    ${OPENGL_gl_LIBRARY}
    ${SDL2_LIBRARIES}
    ${CMAKE_DL_LIBS}
    )



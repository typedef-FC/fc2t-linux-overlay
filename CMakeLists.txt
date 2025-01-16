# cmake settings
cmake_minimum_required(VERSION 3.29)

# project
project(wayland_overlay)

# packages
find_package( SDL3 REQUIRED )
find_package( Vulkan REQUIRED )
find_package( fmt REQUIRED )

# settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# executables
add_executable(wayland_overlay main.cpp)

# include
target_include_directories( wayland_overlay PRIVATE
        ${SDL3_INCLUDE_DIRS}
        "${CMAKE_SOURCE_DIR}/dependencies/include"
)

# linking
target_link_libraries( wayland_overlay PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
        fmt::fmt
        "${CMAKE_SOURCE_DIR}/dependencies/libSDL3_ttf.a"
        freetype
)

cmake_minimum_required(VERSION 3.13)
project(CybikoTUI C CXX ASM)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# adjust this path if your tui/ directory isn't a sibling of CybikoStuff/
add_subdirectory(../../CybikoStuff/lib lib)

add_executable(CybikoTUI
    Main.cpp
)
target_link_libraries(CybikoTUI cybiko_crt0 H8)
make_bootable(CybikoTUI)

cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LEMON_ARCH "x86_64")

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

include_directories(.)
include_directories(./hal/x86_64)

include_directories(../lib/cpp/include)
include_directories(../lib/cxxshim/stage2/include)

add_compile_options(-Wall -Wextra -fno-exceptions -fno-rtti)
add_compile_options(-ffreestanding -nostdlib)
add_compile_options(-mno-mmx -mno-sse -mno-sse2)
add_compile_options(-mcmodel=large -mno-red-zone -fno-pic -fno-pie)

add_compile_options(-O0 -g)
add_compile_options(--target=x86_64-elf)

add_link_options(-nostdlib -nodefaultlibs -no-pie -T ${CMAKE_CURRENT_SOURCE_DIR}/hal/x86_64/link.ld)
add_link_options(-ffreestanding -nostdlib -Wl,--build-id=none)

add_executable(kernel.sys main.cpp)

target_link_options(kernel.sys PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/hal/x86_64/link.ld -v
)

target_sources(kernel.sys PRIVATE
    mm/kmalloc.cpp
    mm/frame_table.cpp
    mm/address_space.cpp

    tests/tests.cpp
    tests/kmalloc.cpp

    util/runtime.cpp

    video/video.cpp
)

add_subdirectory(hal/x86_64)

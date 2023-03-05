project(liblemon)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

add_compile_options(-Wall -Wextra -Wno-write-strings -Wno-missing-braces)
add_compile_options(-fPIC -march=x86-64-v2)

enable_language(ASM_NASM)
if(NOT CMAKE_ASM_NASM_COMPILER_LOADED)
    message(FATAL_ERROR "Failed to find NASM assembler!")
endif()

set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -felf64 -w+gnu-elf-extensions -o <OBJECT> <SOURCE>")

set(LIBLEMON_SRC
    src/Graphics/bitmapfont.cpp
    src/Graphics/Colour.cpp
    src/Graphics/font.cpp
    src/Graphics/graphics.cpp
    src/Graphics/image.cpp
    src/Graphics/Surface.cpp
    src/Graphics/text.cpp
    src/Graphics/texture.cpp
    src/IPC/message.cpp
    src/IPC/interface.cpp
    src/Shell/shell.cpp
    src/cfgparser.cpp
    src/ConfigManager.cpp
    src/IconManager.cpp
    src/lexer.cpp
    src/Logger.cpp
    src/json.cpp
    src/Serializable.cpp
    src/sha.cpp
    src/Unicode.cpp
    src/url.cpp

    src/Lemon/device.cpp
    src/Lemon/framebuffer.cpp
    src/Lemon/info.cpp
    src/Lemon/sharedmem.cpp
    src/Lemon/util.cpp
    src/Lemon/input.cpp
    src/Lemon/waitable.cpp

    src/Graphics/sse2.asm
)

include_directories(include)
find_package(Freetype REQUIRED)
find_package(JPEG REQUIRED)

include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(${JPEG_INCLUDE_DIR})

add_library(lemon SHARED ${LIBLEMON_SRC})
target_link_options(lemon PUBLIC -lz -lpng)

target_link_libraries(lemon PUBLIC ${FREETYPE_LIBRARIES})
target_link_libraries(lemon PUBLIC ${JPEG_LIBRARIES})

include(GNUInstallDirs)

install(TARGETS lemon)
install(DIRECTORY include/lemon DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

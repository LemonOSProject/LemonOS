project(liblemongui)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

add_compile_options(-Wall -Wextra -Wno-write-strings -Wno-missing-braces)

set(LIBLEMONGUI_SRC
    src/FileView.cpp
    src/FileDialog.cpp
    src/Image.cpp
    src/MessageBox.cpp
    src/Theme.cpp
    src/Widgets.cpp
    src/Window.cpp
    src/WindowServer.cpp
)

include_directories(include)

add_library(lemongui SHARED ${LIBLEMONGUI_SRC})
target_link_libraries(lemongui PUBLIC lemon)
target_link_options(lemongui PUBLIC -lz -lpng)

include(GNUInstallDirs)

install(TARGETS lemongui)
install(DIRECTORY include/lemon DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

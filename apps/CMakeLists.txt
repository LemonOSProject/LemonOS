project(lemon-system)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

add_compile_options(-Werror -Wall -Wextra -Wno-write-strings -Wno-missing-braces -Wno-unused-parameter)
add_compile_options(-march=x86-64-v2)

add_subdirectory(core)
add_subdirectory(demos)
add_subdirectory(games)
add_subdirectory(lsh)
add_subdirectory(media)
add_subdirectory(servers)
add_subdirectory(system)
add_subdirectory(tests)
add_subdirectory(utilities)

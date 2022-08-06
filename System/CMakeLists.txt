project(lemon-system)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

add_compile_options(-Werror -Wall -Wextra -Wno-write-strings -Wno-missing-braces)
add_compile_options(-march=x86-64-v2)

set(lemond_SRC
    Lemond/main.cpp
)

set(netgov_SRC
    NetworkGovernor/main.cpp
)

set(login_SRC
    Login/main.cpp
)

set(kmod_SRC
    KMod/Main.cpp
)

set(lemonwm_SRC
    LemonWM/Compositor.cpp
    LemonWM/Input.cpp
    LemonWM/Main.cpp
    LemonWM/Window.cpp
    LemonWM/WM.cpp
)

add_link_options(-llemon)

add_executable(lemond.lef ${lemond_SRC})
add_executable(netgov.lef ${netgov_SRC})
add_executable(kmod.lef ${kmod_SRC})

add_executable(login.lef ${login_SRC})
target_link_options(login.lef PUBLIC -llemongui)

add_executable(lemonwm.lef ${lemonwm_SRC})
target_link_options(lemonwm.lef PUBLIC -llemongui)

install(TARGETS lemond.lef netgov.lef login.lef kmod.lef lemonwm.lef
    RUNTIME DESTINATION lemon)

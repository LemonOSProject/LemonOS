add_executable(init.lef lemond/main.cpp)
target_link_options(init.lef PUBLIC -llemon)

add_executable(netgov.lef netgovernor/main.cpp)
target_link_options(netgov.lef PUBLIC -llemon)

add_executable(lemonwm.lef
    lemonwm/Compositor.cpp
    lemonwm/Input.cpp
    lemonwm/Main.cpp
    lemonwm/Window.cpp
    lemonwm/WM.cpp
)
target_link_options(lemonwm.lef PUBLIC -llemon -llemongui)

add_executable(shell.lef
    shell/main.cpp
    shell/menu.cpp
    shell/shell.cpp
)
target_link_options(shell.lef PUBLIC -llemon -llemongui)

install(TARGETS
    shell.lef
)

install(TARGETS
    init.lef
    netgov.lef
    lemonwm.lef
    RUNTIME DESTINATION lemon
)

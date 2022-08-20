add_executable(fterm.lef
    fterm/main.cpp
    fterm/input.cpp
)
target_link_options(fterm.lef PUBLIC -llemon)

add_executable(login.lef login/main.cpp)
target_link_options(login.lef PUBLIC -llemon -llemongui)

install(TARGETS login.lef fterm.lef
    RUNTIME DESTINATION lemon)

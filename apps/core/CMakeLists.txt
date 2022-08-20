# TODO: liblemon CMake module
add_link_options(-llemon -llemongui)

add_executable(fileman.lef fileman/main.cpp)
add_executable(terminal.lef terminal/main.cpp)
add_executable(textedit.lef textedit/exttextbox.cpp textedit/main.cpp)
add_executable(sysinfo.lef sysinfo/main.cpp)
add_executable(lemonmonitor.lef lemonmonitor/main.cpp)
add_executable(devices.lef devices/main.cpp)
add_executable(welcome.lef welcome/main.cpp)

install(TARGETS
    fileman.lef
    terminal.lef
    textedit.lef
    sysinfo.lef
    lemonmonitor.lef
    devices.lef
    welcome.lef
)

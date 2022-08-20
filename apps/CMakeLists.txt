project(lemon-apps)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

add_compile_options(-Werror -Wall -Wextra -Wno-write-strings -Wno-missing-braces -Wno-unused-parameter)
add_compile_options(-march=x86-64-v2)

set(fileman_SRC
    FileManager/main.cpp
)

set(lsh_SRC
    LSh/main.cpp
)

set(shell_SRC
    Shell/main.cpp
    Shell/shell.cpp
    Shell/menu.cpp
)

set(snake_SRC
    Snake/main.cpp
)

set(terminal_SRC
    Terminal/main.cpp
)

set(textedit_SRC
    TextEdit/exttextbox.cpp
    TextEdit/main.cpp
)

set(sysinfo_SRC
    SysInfo/main.cpp
)

set(imgview_SRC
    ImgView/main.cpp
)

set(run_SRC
    Run/main.cpp
)

set(graphicstest_SRC
    GraphicsTest/main.cpp
)

set(guitest_SRC
    GUITest/main.cpp
)

set(lemonmonitor_SRC
    LemonMonitor/main.cpp
)

set(jsondump_SRC
    JSONDump/main.cpp
)

set(minesweeper_SRC
    Minesweeper/main.cpp
)

set(steal_SRC
    Steal/main.cpp
)

set(devicemanager_SRC
    DeviceManager/main.cpp
)

set(tests_SRC
    Tests/Main.cpp
)

set(audioplayer_SRC
    AudioPlayer/AudioContext.cpp
    AudioPlayer/Main.cpp
)

set(videoplayer_SRC
    VideoPlayer/StreamContext.cpp
    VideoPlayer/Main.cpp
)

set(welcome_SRC
    Welcome/Main.cpp
)

add_link_options(-llemon -llemongui)

add_executable(fileman.lef ${fileman_SRC})
add_executable(shell.lef ${shell_SRC})
add_executable(snake.lef ${snake_SRC})
add_executable(terminal.lef ${terminal_SRC})
add_executable(textedit.lef ${textedit_SRC})
add_executable(imgview.lef ${imgview_SRC})
add_executable(sysinfo.lef ${sysinfo_SRC})
add_executable(gfxtest.lef ${graphicstest_SRC})
add_executable(guitest.lef ${guitest_SRC})
add_executable(run.lef ${run_SRC})
add_executable(lemonmonitor.lef ${lemonmonitor_SRC})
add_executable(jsondump.lef ${jsondump_SRC})
add_executable(minesweeper.lef ${minesweeper_SRC})
add_executable(devmgr.lef ${devicemanager_SRC})
add_executable(welcome.lef ${welcome_SRC})

add_executable(ipctest.lef IPCTest/main.cpp)
add_executable(sigtest.lef SignalTest/main.c)
add_executable(tests.lef ${tests_SRC})

add_executable(audioplayer.lef ${audioplayer_SRC})
add_executable(videoplayer.lef ${videoplayer_SRC})
target_link_options(audioplayer.lef PUBLIC
    -lavcodec -lavformat -lavutil -lswresample -lswscale)
target_link_options(videoplayer.lef PUBLIC
    -lavcodec -lavformat -lavutil -lswresample -lswscale)


add_executable(lsh ${lsh_SRC})
add_executable(steal ${steal_SRC})
target_link_options(steal PUBLIC -lssl -lcrypto)

install(TARGETS
    fileman.lef
    lsh
    shell.lef
    snake.lef
    terminal.lef
    textedit.lef
    imgview.lef
    sysinfo.lef
    gfxtest.lef
    guitest.lef
    run.lef
    lemonmonitor.lef
    jsondump.lef
    minesweeper.lef
    steal
    devmgr.lef
    audioplayer.lef
    videoplayer.lef
    welcome.lef

    ipctest.lef
    sigtest.lef
    tests.lef
)

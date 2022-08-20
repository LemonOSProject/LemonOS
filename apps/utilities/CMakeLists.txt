add_executable(cat cat.cpp)
add_executable(echo echo.cpp)
add_executable(rm rm.cpp)
add_executable(ls ls.cpp)
add_executable(uname uname.cpp)
add_executable(hexdump hexdump.cpp)

add_executable(ps ps.cpp)
target_link_options(ps PUBLIC -llemon)

add_executable(playaudio playaudio.cpp)
target_link_options(playaudio PUBLIC
    -lavcodec -lavformat -lavutil -lswresample -lswscale)

add_executable(lemonfetch lemonfetch.cpp)
target_link_options(lemonfetch PUBLIC -llemon -llemongui)

add_executable(steal steal/main.cpp)
target_link_options(steal PUBLIC -lssl -lcrypto -llemon)

install(TARGETS
    cat
    echo
    hexdump
    lemonfetch
    ls
    ps
    playaudio
    rm
    steal
    uname
)

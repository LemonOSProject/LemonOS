project(lemon-utils)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options($<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>)

add_compile_options(-Werror -Wall -Wextra -Wno-write-strings -Wno-missing-braces -Wno-unused-parameter)
add_compile_options(-march=x86-64-v2)

set(cat_SRC
    cat.cpp
)

set(echo_SRC
    echo.cpp
)

set(lemonfetch_SRC
    lemonfetch.cpp
)

set(ls_SRC
    ls.cpp
)

set(rm_SRC
    rm.cpp
)

set(uname_SRC
    uname.cpp
)

set(hexdump_SRC
    hexdump.cpp
)

set(ps_SRC
    ps.cpp
)

set(playaudio_SRC
    playaudio.cpp
)

add_executable(cat ${cat_SRC})
add_executable(echo ${echo_SRC})
add_executable(rm ${rm_SRC})
add_executable(ls ${ls_SRC})
add_executable(uname ${uname_SRC})
add_executable(hexdump ${hexdump_SRC})

add_executable(ps ${ps_SRC})
target_link_options(ps PUBLIC -llemon)

add_executable(playaudio ${playaudio_SRC})
target_link_options(playaudio PUBLIC
    -lavcodec -lavformat -lavutil -lswresample -lswscale)

add_executable(lemonfetch ${lemonfetch_SRC})
target_link_options(lemonfetch PUBLIC -llemon -llemongui)

install(TARGETS
    cat
    echo
    rm
    lemonfetch
    ls
    uname
    hexdump
    ps
    playaudio
)

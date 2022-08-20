set(audioplayer_SRC
    audioplayer/AudioContext.cpp
    audioplayer/Main.cpp
)

set(videoplayer_SRC
    videoplayer/StreamContext.cpp
    videoplayer/Main.cpp
)

add_executable(imgview.lef imgview/main.cpp)

target_link_options(imgview.lef PUBLIC
    -llemon -llemongui)

add_executable(audioplayer.lef ${audioplayer_SRC})
add_executable(videoplayer.lef ${videoplayer_SRC})
target_link_options(audioplayer.lef PUBLIC
    -lavcodec -lavformat -lavutil -lswresample -lswscale -llemon -llemongui)
target_link_options(videoplayer.lef PUBLIC
    -lavcodec -lavformat -lavutil -lswresample -lswscale -llemon -llemongui)

install(TARGETS
    audioplayer.lef
    videoplayer.lef
    imgview.lef
)

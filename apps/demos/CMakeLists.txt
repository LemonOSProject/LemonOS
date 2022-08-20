add_link_options(-llemon -llemongui)

add_executable(gfxdemo.lef graphics/main.cpp)
add_executable(guidemo.lef gui/main.cpp)

install(TARGETS
    gfxdemo.lef
    guidemo.lef
)

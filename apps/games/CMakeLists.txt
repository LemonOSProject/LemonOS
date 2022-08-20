add_link_options(-llemon -llemongui)

add_executable(minesweeper.lef minesweeper/main.cpp)
add_executable(snake.lef snake/main.cpp)

install(TARGETS
    minesweeper.lef
    snake.lef
)
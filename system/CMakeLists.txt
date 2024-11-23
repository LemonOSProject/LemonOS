add_compile_options(-ffreestanding -nostdlib)

add_compile_options(-O0 -g)
add_compile_options(--target=x86_64-elf)

add_executable(system main.cpp)
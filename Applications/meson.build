project('Lemon Applications')

add_languages('c', 'cpp')

fileman_src = [
    'FileManager/main.cpp',
]

init_src = [
    'Init/main.cpp',
]

lsh_src = [
    'LSh/main.cpp',
]

shell_src = [
    'Shell/main.cpp',
]

snake_src = [
    'Snake/main.cpp',
]

terminal_src = [
    'Terminal/main.cpp',
]

textedit_src = [
    'TextEdit/main.cpp',
]

application_cpp_args = [
    '-fno-exceptions',
]

executable('fileman.lef', fileman_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../InitrdWriter/Contents')
executable('init.lef', init_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../InitrdWriter/Contents')
executable('lsh.lef', lsh_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../InitrdWriter/Contents')
executable('shell.lef', shell_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../InitrdWriter/Contents')
executable('snake.lef', snake_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../InitrdWriter/Contents')
executable('terminal.lef', terminal_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../InitrdWriter/Contents')
executable('textedit.lef', textedit_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../InitrdWriter/Contents')
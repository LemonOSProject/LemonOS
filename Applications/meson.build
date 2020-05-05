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

sdltest_src = [
    'SDLTest/main.cpp',
]

lemonpaint_src = [
    'LemonPaint/main.cpp',
    'LemonPaint/canvas.cpp',
    'LemonPaint/brush.cpp',
]

imgview_src = [
    'ImgView/main.cpp',
]

application_cpp_args = [
    '-fno-exceptions',
]

executable('fileman.lef', fileman_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('init.lef', init_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('lsh.lef', lsh_src, cpp_args : application_cpp_args, link_args : ['-llemon'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('shell.lef', shell_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('snake.lef', snake_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('terminal.lef', terminal_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('textedit.lef', textedit_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('lemonpaint.lef', lemonpaint_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('imgview.lef', imgview_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('sdltest.lef', sdltest_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lSDL2', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
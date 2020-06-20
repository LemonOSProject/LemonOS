project('Lemon Applications')

add_languages('c', 'cpp')

fileman_src = [
    'FileManager/main.cpp',
]

lemonwm_src = [
    'LemonWM/main.cpp',
    'LemonWM/window.cpp',
    'LemonWM/input.cpp',
    'LemonWM/compositor.cpp',
    'LemonWM/wm.cpp',
]

lsh_src = [
    'LSh/main.cpp',
]

shell_src = [
    'Shell/main.cpp',
    'Shell/shell.cpp',
]

snake_src = [
    'Snake/main.cpp',
]

terminal_src = [
    'Terminal/main.cpp',
]

textedit_src = [
    'TextEdit/exttextbox.cpp',
    'TextEdit/main.cpp',
]

sysinfo_src = [
    'SysInfo/main.cpp',
]

lemonpaint_src = [
    'LemonPaint/main.cpp',
    'LemonPaint/canvas.cpp',
    'LemonPaint/brush.cpp',
]

imgview_src = [
    'ImgView/main.cpp',
]

solitaire_src = [
    'Solitaire/main.cpp',
]

fstest_src = [
    'FSTest/main.cpp',
]

solitaire_src = [
    'Solitaire/main.cpp',
]

application_cpp_args = [
    #'',
]

guitest_src = [
    'GUITest/main.cpp',
]

executable('fileman.lef', fileman_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('init.lef', lemonwm_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('lsh.lef', lsh_src, cpp_args : application_cpp_args, link_args : ['-llemon'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('shell.lef', shell_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('snake.lef', snake_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('terminal.lef', terminal_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('textedit.lef', textedit_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
#executable('lemonpaint.lef', lemonpaint_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
#executable('imgview.lef', imgview_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('sysinfo.lef', sysinfo_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
#executable('solitaire.lef', solitaire_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('fstest.lef', fstest_src, cpp_args : application_cpp_args, install : true, install_dir : meson.current_source_dir() + '/../Initrd')
executable('guitest.lef', guitest_src, cpp_args : application_cpp_args, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install : true, install_dir : meson.current_source_dir() + '/../Initrd')
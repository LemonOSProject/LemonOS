project('Lemon System',
    default_options : ['warning_level=2', 'cpp_std=c++2a', 'optimization=3', 'werror=true'])

add_languages('c', 'cpp')

lemond_src = [
    'Lemond/main.cpp',
]

lemonwm_src = [
    'LemonWM/main.cpp',
    'LemonWM/window.cpp',
    'LemonWM/input.cpp',
    'LemonWM/compositor.cpp',
    'LemonWM/wm.cpp',
]

netgov_src = [
    'NetworkGovernor/main.cpp',
]

login_src = [
    'Login/main.cpp',
]

fterm_src = [
    'FTerm/main.cpp',
    'FTerm/input.cpp',
]

kmod_src = [
    'KMod/Main.cpp',
]

add_project_arguments(['-Wno-write-strings', '-Wno-unused-parameter', '-Wno-missing-braces', '-Wno-missing-field-initializers'], language : 'cpp')

executable('init.lef', lemond_src, link_args : ['-llemon'], install_dir : 'lemon/', install : true)
executable('lemonwm.lef', lemonwm_src, link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install_dir : 'lemon/', install : true)
executable('netgov.lef', netgov_src, install_dir : 'lemon/', install : true)
executable('login.lef', login_src, link_args : ['-llemon', '-lfreetype'], install_dir : 'lemon/', install : true)
executable('fterm.lef', fterm_src, link_args : ['-llemon', '-lfreetype'], install_dir : meson.current_source_dir() + '/../Initrd', install : true)
executable('kmod.lef', kmod_src, link_args : ['-llemon'], install_dir : 'lemon/', install : true)

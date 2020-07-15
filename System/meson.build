project('Lemon System')

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

loginfrontend_src = [
    'LoginFrontend/main.cpp',
]

executable('init.lef', lemond_src, link_args : ['-llemon'], install_dir : 'lemon/', install : true)
executable('lemonwm.lef', lemonwm_src, cpp_args : '-O3', link_args : ['-llemon', '-lfreetype', '-lz', '-lpng'], install_dir : 'lemon/', install : true)
executable('netgov.lef', netgov_src, install_dir : 'lemon/', install : true)
executable('login.lef', loginfrontend_src, link_args : ['-llemon', '-lfreetype'], install_dir : 'lemon/', install : true)

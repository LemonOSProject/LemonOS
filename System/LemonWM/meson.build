lemonwm_src = [
    'Compositor.cpp',
    'Input.cpp',
    'Main.cpp',
    'Window.cpp',
    'WM.cpp',
]
 
executable('lemonwm.lef', lemonwm_src,
    cpp_args: system_args,
    link_args : ['-llemon', '-llemongui'],
    install_dir : 'lemon/', install : true)

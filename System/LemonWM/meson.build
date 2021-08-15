lemonwm_src = [
    'Compositor.cpp',
    'Main.cpp',
    'Window.cpp',
    'WM.cpp',
]
 
executable('lemonwm.lef', lemonwm_src,
    cpp_args: system_args,
    dependencies: [system_dependencies, libgui_dep],
    link_args : ['-lfreetype', '-lz', '-lpng'],
    install_dir : 'lemon/', install : true)

add_languages('c', 'cpp')

system_dependencies = [
    liblemon_dep
]

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

system_args = ['-Wno-write-strings', '-Wno-unused-parameter', '-Wno-missing-braces', '-Wno-missing-field-initializers']

executable('init.lef', lemond_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    install_dir : 'lemon/', install : true)
executable('lemonwm.lef', lemonwm_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'],
    install_dir : 'lemon/', install : true)
executable('netgov.lef', netgov_src,
    cpp_args: system_args,
    install_dir : 'lemon/', install : true)
executable('login.lef', login_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    link_args : ['-lfreetype'],
    install_dir : 'lemon/', install : true)
executable('fterm.lef', fterm_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    link_args : ['-lfreetype'],
    install_dir : meson.current_source_dir() + '/../Initrd', install : true)
executable('kmod.lef', kmod_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    install_dir : 'lemon/', install : true)

add_languages('c', 'cpp')

system_dependencies = [
    liblemon_dep
]

system_args = ['-Wno-write-strings', '-Wno-unused-parameter', '-Wno-missing-braces', '-Wno-missing-field-initializers']

subdir('LemonWM')

lemond_src = [
    'Lemond/main.cpp',
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

system_init = executable('init.lef', lemond_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    install_dir : 'lemon/', install : true)
system_netgov = executable('netgov.lef', netgov_src,
    cpp_args: system_args,
    install_dir : 'lemon/', install : true)
system_login = executable('login.lef', login_src,
    cpp_args: system_args,
    dependencies: [ system_dependencies, libgui_dep ],
    link_args : ['-lfreetype'],
    install_dir : 'lemon/', install : true)
system_fterm = executable('fterm.lef', fterm_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    link_args : ['-lfreetype'],
    install_dir : meson.current_source_dir() + '/../Initrd', install : true)
system_kmod = executable('kmod.lef', kmod_src,
    cpp_args: system_args,
    dependencies: system_dependencies,
    install_dir : 'lemon/', install : true)

system_targets = [
    system_init,
    system_netgov,
    system_login,
    system_fterm,
    system_kmod
]
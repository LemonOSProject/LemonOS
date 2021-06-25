add_languages('c', 'cpp')

cat_src = [
    'cat.cpp',
]

echo_src = [
    'echo.cpp',
]

ls_src = [
    'ls.cpp',
]

rm_src = [
    'rm.cpp',
]

uname_src = [
    'uname.cpp',
]

hexdump_src = [
    'hexdump.cpp',
]

ps_src = [
    'ps.cpp',
]

utils_cpp_args = [
    '-Wno-unused-parameter',
]

executable('cat.lef', cat_src, cpp_args : utils_cpp_args, install : true)
executable('echo.lef', echo_src, cpp_args : utils_cpp_args, install : true)
executable('rm.lef', rm_src, cpp_args : utils_cpp_args, install : true)
executable('ls.lef', ls_src, cpp_args : utils_cpp_args, install : true)
executable('uname.lef', uname_src, cpp_args : utils_cpp_args, install : true)
executable('hexdump.lef', hexdump_src, cpp_args : utils_cpp_args, install : true)
executable('ps.lef', ps_src, cpp_args : utils_cpp_args,
    dependencies: liblemon_dep,
    install : true)

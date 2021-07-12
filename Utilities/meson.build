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

executable('cat', cat_src, cpp_args : utils_cpp_args, install : true)
executable('echo', echo_src, cpp_args : utils_cpp_args, install : true)
executable('rm', rm_src, cpp_args : utils_cpp_args, install : true)
executable('ls', ls_src, cpp_args : utils_cpp_args, install : true)
executable('uname', uname_src, cpp_args : utils_cpp_args, install : true)
executable('hexdump', hexdump_src, cpp_args : utils_cpp_args, install : true)
executable('ps', ps_src, cpp_args : utils_cpp_args,
    dependencies: liblemon_dep,
    install : true)

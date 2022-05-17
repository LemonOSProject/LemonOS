project('lemon-uitls',
    default_options : ['warning_level=2', 'cpp_std=c++2a', 'optimization=g', 'werror=true'])
add_languages('c', 'cpp')

cat_src = [
    'cat.cpp',
]

echo_src = [
    'echo.cpp',
]

lemonfetch_src = [
    'lemonfetch.cpp',
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

playaudio_src = [
    'playaudio.cpp',
]

utils_cpp_args = [
    '-Wno-unused-parameter',
    '-Wno-missing-braces'
]

executable('cat', cat_src, cpp_args : utils_cpp_args, install : true)
executable('echo', echo_src, cpp_args : utils_cpp_args, install : true)
executable('rm', rm_src, cpp_args : utils_cpp_args, install : true)
executable('lemonfetch', lemonfetch_src, cpp_args : utils_cpp_args,
    link_args : ['-llemon', '-llemongui'],
    install : true)
executable('ls', ls_src, cpp_args : utils_cpp_args, install : true)
executable('uname', uname_src, cpp_args : utils_cpp_args, install : true)
executable('hexdump', hexdump_src, cpp_args : utils_cpp_args, install : true)
executable('ps', ps_src, cpp_args : utils_cpp_args,
    link_args : ['-llemon'],
    install : true)
executable('playaudio', playaudio_src, cpp_args : utils_cpp_args,
    link_args : ['-llemon', '-lavcodec', '-lavutil', '-lavformat', '-lswresample'],
    install : true)

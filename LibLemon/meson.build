liblemon_include_dirs = [
    include_directories('include')
]

add_languages('c', 'cpp')

liblemon_cpp_args = ['-Wno-write-strings', '-Wno-missing-braces',
    '-fPIC',
    '-m64' ]

liblemon_cpp_files = [
    'src/gfx/font.cpp',
    'src/gfx/bitmapfont.cpp',
    'src/gfx/graphics.cpp',
    'src/gfx/image.cpp',
    'src/gfx/surface.cpp',
    'src/gfx/text.cpp',
    'src/gfx/texture.cpp',

    'src/ipc/message.cpp',
	'src/ipc/interface.cpp',

    'src/shell/shell.cpp',

    'src/cfgparser.cpp',
    'src/lexer.cpp',
    'src/json.cpp',
    'src/sha.cpp',
    'src/url.cpp',
]

liblemon_asm_files = [
    'src/gfx/sse2.asm',
]

subdir('src/lemon')

prefix = get_option('prefix')
install_subdir('include', install_dir: prefix)

liblemon = static_library('lemon', [asmg.process(liblemon_asm_files), liblemon_cpp_files],
    include_directories : liblemon_include_dirs,
    cpp_args: liblemon_cpp_args,
    link_args: ['-lfreetype', '-lstdc++', '-lz', '-lpng'],
    install: true)

liblemon_dep = declare_dependency(
    link_with: liblemon,
    include_directories: liblemon_include_dirs,
)
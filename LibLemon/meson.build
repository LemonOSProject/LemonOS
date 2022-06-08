project('liblemon',
    default_options : ['warning_level=2', 'cpp_std=c++2a', 'optimization=3', 'werror=true'])
add_languages('c', 'cpp')

liblemon_include_dirs = [
    include_directories('include')
]

add_languages('c', 'cpp')

nasm = find_program('nasm')
asmg = generator(nasm,
    output : '@BASENAME@.asm.o',
    arguments : [
        '-f', 'elf64',
        '-g', '-F', 'dwarf', '-w+gnu-elf-extensions',
        '@INPUT@',
        '-o', '@OUTPUT@'])

liblemon_cpp_args = ['-Wno-write-strings', '-Wno-missing-braces',
    '-fPIC',
    '-m64' ]

liblemon_cpp_files = [
    'src/Graphics/bitmapfont.cpp',
    'src/Graphics/Colour.cpp',
    'src/Graphics/font.cpp',
    'src/Graphics/graphics.cpp',
    'src/Graphics/image.cpp',
    'src/Graphics/Surface.cpp',
    'src/Graphics/text.cpp',
    'src/Graphics/texture.cpp',

    'src/IPC/message.cpp',
	'src/IPC/interface.cpp',

    'src/Shell/shell.cpp',

    'src/cfgparser.cpp',
    'src/ConfigManager.cpp',
    'src/IconManager.cpp',
    'src/lexer.cpp',
    'src/Logger.cpp',
    'src/json.cpp',
    'src/Serializable.cpp',
    'src/sha.cpp',
    'src/Unicode.cpp',
    'src/url.cpp',
]

liblemon_asm_files = [
    'src/Graphics/sse2.asm',
]

subdir('src/Lemon')

prefix = get_option('prefix')
install_subdir('include', install_dir: prefix)

ft_dep = dependency('freetype2')

liblemon = library('lemon', [asmg.process(liblemon_asm_files), liblemon_cpp_files],
    dependencies: ft_dep,
    include_directories : liblemon_include_dirs,
    cpp_args: liblemon_cpp_args,
    link_args: ['-lfreetype', '-lstdc++', '-lz', '-lpng'],
    install: true)

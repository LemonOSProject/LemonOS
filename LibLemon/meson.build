project('LibLemon')

liblemon_include_dirs = [
    include_directories('include'),
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

liblemon_cpp_args = ['-Wno-write-strings', '-Wno-pointer-arith', '-Wall', '-Wextra', '-Wno-address',
    '-fPIC',
    '-m64',
    '-O3',
    '-fno-exceptions', '-std=c++14']

add_project_arguments(liblemon_cpp_args, language : ['c', 'cpp'])

cpp_files = [
    'src/fb.cpp',
    'src/filesystem.cpp',
    'src/ipc.cpp',
    'src/itoa.cpp',
    'src/runtime.cpp',
    'src/spawn.cpp',
    'src/info.cpp',
    'src/sharedmem.cpp',

    'src/gfx/font.cpp',
    'src/gfx/graphics.cpp',
    'src/gfx/image.cpp',
    'src/gfx/surface.cpp',
    'src/gfx/text.cpp',

    'src/gfx/window/filedialog.cpp',
    'src/gfx/window/messagebox.cpp',
    'src/gfx/window/widgets.cpp',
    'src/gfx/window/fileview.cpp',
    'src/gfx/window/window.cpp',
]

asm_files = [
    'src/gfx/sse2.asm',
]

prefix = get_option('prefix')
install_subdir('include', install_dir: prefix)

static_library('lemon', [asmg.process(asm_files), cpp_files],
    include_directories : liblemon_include_dirs,
    link_args: ['-lfreetype', '-lstdc++', '-lz', '-lpng'],
    install: true)
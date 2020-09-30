project('LibLemon', default_options : ['cpp_std=c++17', 'optimization=3', 'warning_level=2', 'werror=true'])

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

liblemon_cpp_args = ['-Wno-write-strings',
    '-fPIC',
    '-m64' ]

add_project_arguments(liblemon_cpp_args, language : ['c', 'cpp'])

cpp_files = [
    'src/gfx/font.cpp',
    'src/gfx/bitmapfont.cpp',
    'src/gfx/graphics.cpp',
    'src/gfx/image.cpp',
    'src/gfx/surface.cpp',
    'src/gfx/text.cpp',

    'src/ipc/msghandler.cpp',
    'src/ipc/message.cpp',

    'src/gui/window.cpp',
    'src/gui/widgets.cpp',
    'src/gui/colours.cpp',
    'src/gui/fileview.cpp',
    'src/gui/filedialog.cpp',
    'src/gui/messagebox.cpp',

    'src/shell/shell.cpp',

    'src/cfgparser.cpp',
    'src/sha.cpp',
]

asm_files = [
    'src/gfx/sse2.asm',
]

if host_machine.system() == 'lemon'
    subdir('src/lemon')
else
    error('System ' + host_machine.system() + ' not supported!')
endif
    

prefix = get_option('prefix')
install_subdir('include', install_dir: prefix)

static_library('lemon', [asmg.process(asm_files), cpp_files],
    include_directories : liblemon_include_dirs,
    link_args: ['-lfreetype', '-lstdc++', '-lz', '-lpng'],
    install: true)

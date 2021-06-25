liblemon_include_dirs = [
    include_directories('include')
]

add_languages('c', 'cpp')

liblemon_cpp_args = ['-Wno-write-strings', '-Wno-missing-braces',
    '-fPIC',
    '-m64' ]

cpp_files = [
    'src/gfx/font.cpp',
    'src/gfx/bitmapfont.cpp',
    'src/gfx/graphics.cpp',
    'src/gfx/image.cpp',
    'src/gfx/surface.cpp',
    'src/gfx/text.cpp',
    'src/gfx/texture.cpp',

    'src/ipc/message.cpp',
	'src/ipc/interface.cpp',

    'src/gui/window.cpp',
    'src/gui/widgets.cpp',
    'src/gui/colours.cpp',
    'src/gui/fileview.cpp',
    'src/gui/filedialog.cpp',
    'src/gui/image.cpp',
    'src/gui/messagebox.cpp',

    'src/shell/shell.cpp',

    'src/cfgparser.cpp',
    'src/lexer.cpp',
    'src/json.cpp',
    'src/sha.cpp',
    'src/url.cpp',
]

asm_files = [
    'src/gfx/sse2.asm',
]

subdir('src/lemon')

prefix = get_option('prefix')
install_subdir('include',
    install_dir: 'include')

liblemon = static_library('lemon', [asmg.process(asm_files), cpp_files],
    include_directories : liblemon_include_dirs,
    cpp_args: liblemon_cpp_args,
    link_args: ['-lfreetype', '-lstdc++', '-lz', '-lpng'],
    install: true)

liblemon_dep = declare_dependency(
    link_with: liblemon,
    include_directories: liblemon_include_dirs,
)
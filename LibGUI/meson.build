project('liblemongui',
    default_options : ['warning_level=2', 'cpp_std=c++2a', 'optimization=g', 'werror=true'])
add_languages('c', 'cpp')

libgui_include_dirs = [
    include_directories('include')
]

add_languages('c', 'cpp')

libgui_cpp_args = ['-Wno-missing-braces',
    '-fPIC',
    '-m64' ]

libgui_cpp_files = [
    'src/FileView.cpp',
    'src/FileDialog.cpp',
    'src/Image.cpp',
    'src/MessageBox.cpp',
    'src/Theme.cpp',
    'src/Widgets.cpp',
    'src/Window.cpp',
    'src/WindowServer.cpp',
]

prefix = get_option('prefix')
install_subdir('include', install_dir: prefix)

libgui = library('lemongui', [libgui_cpp_files],
    include_directories : libgui_include_dirs,
    cpp_args: libgui_cpp_args,
    link_args: ['-lfreetype', '-lz', '-lpng', '-llemon'],
    install: true)

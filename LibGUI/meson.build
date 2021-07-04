libgui_include_dirs = [
    include_directories('include')
]

add_languages('c', 'cpp')

libgui_cpp_args = ['-Wno-missing-braces',
    '-fPIC',
    '-m64' ]

libgui_cpp_files = [
    'src/Colours.cpp',
    'src/FileView.cpp',
    'src/FileDialog.cpp',
    'src/Image.cpp',
    'src/MessageBox.cpp',
    'src/Widgets.cpp',
    'src/Window.cpp',
    'src/WindowServer.cpp',
]

prefix = get_option('prefix')
install_subdir('include', install_dir: prefix)

libgui = static_library('lemongui', [libgui_cpp_files],
    include_directories : libgui_include_dirs,
    cpp_args: libgui_cpp_args,
    dependencies: [ liblemon_dep ],
    link_args: ['-lfreetype', '-lz', '-lpng'],
    install: true)

libgui_dep = declare_dependency(
    link_with: libgui,
    include_directories: libgui_include_dirs,
)
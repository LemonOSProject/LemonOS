libgui_include_dirs = [
    include_directories('include')
]

add_languages('c', 'cpp')

libgui_cpp_args = ['-Wno-missing-braces',
    '-fPIC',
    '-m64' ]

libgui_cpp_files = [
    'src/window.cpp',
    'src/widgets.cpp',
    'src/colours.cpp',
    'src/fileview.cpp',
    'src/filedialog.cpp',
    'src/image.cpp',
    'src/messagebox.cpp',
]

prefix = get_option('prefix')
install_subdir('include',
    install_dir: 'include')

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
add_languages('c', 'cpp')

module_c_args = kernel_c_args

module_cpp_args = kernel_cpp_args

module_include_dirs = [
    include_directories('../include'),
    include_directories('../include/Arch/x86_64'),
]

executable('ext2fs.sys', ['Ext2/Main.cpp'],
    c_args: module_c_args, cpp_args: [ module_c_args, module_cpp_args ],
    include_directories : module_include_dirs, link_args : [ '-r', module_c_args ])
executable('hdaudio.sys', ['HDAudio/Main.cpp'],
    c_args: module_c_args, cpp_args: [ module_c_args, module_cpp_args ],
    include_directories : module_include_dirs, link_args : [ '-r', module_c_args ])
executable('e1k.sys', ['Intel8254x/Main.cpp'],
    c_args: module_c_args, cpp_args: [ module_c_args, module_cpp_args ],
    include_directories : module_include_dirs, link_args : [ '-r', module_c_args ])
executable('testmodule.sys', ['TestModule/Main.cpp'],
    c_args: module_c_args, cpp_args: [ module_c_args, module_cpp_args ],
    include_directories : module_include_dirs, link_args : [ '-r', module_c_args ])

add_languages('c', 'cpp')

module_c_args = [
    '-ffreestanding', '-nostdlib',
    '-mcmodel=large', '-mno-red-zone', '-fPIC',
    '-mno-mmx', '-mno-sse', '-mno-sse2'
]

module_cpp_args = [
    '-fno-exceptions', '-fno-rtti',
]

module_include_dirs = [
    include_directories('../include'),
    include_directories('../subprojects/lai/include'),
    include_directories('../include/Arch/x86_64'),
]

add_project_arguments(module_c_args, language : 'c')
add_project_arguments(module_c_args, language : 'cpp')
add_project_arguments(module_cpp_args, language : 'cpp')

executable('testmodule.sys', ['TestModule/Main.cpp'], include_directories : module_include_dirs, link_args : [ '-r', module_c_args ])
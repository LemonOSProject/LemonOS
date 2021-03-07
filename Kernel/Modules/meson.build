add_languages('c', 'cpp')

module_c_args = [
    '-ffreestanding', '-nostdlib',
    '-mcmodel=large', '-mno-red-zone', '-fPIC',
    '-mno-mmx', '-mno-sse', '-mno-sse2'
]

module_cpp_args = [
    '-fno-exceptions', '-fno-rtti',
]

add_project_arguments(module_c_args, language : 'c')
add_project_arguments(module_c_args, language : 'cpp')
add_project_arguments(module_cpp_args, language : 'cpp')

shared_library('testmodule', ['TestModule/Main.cpp'], name_prefix: '', name_suffix: 'sys')
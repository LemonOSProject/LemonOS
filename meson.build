project('Lemon OS',
    default_options : ['warning_level=2', 'cpp_std=c++2a', 'optimization=0', 'werror=true'])
add_languages('c', 'cpp')

if host_machine.system() != 'lemon'
    error('Invalid host system: \'' + host_machine.system() + '\', should be \'lemon\'')
endif

nasm = find_program('nasm')
asmg = generator(nasm,
    output : '@BASENAME@.asm.o',
    arguments : [
        '-f', 'elf64',
        '-g', '-F', 'dwarf', '-w+gnu-elf-extensions',
        '@INPUT@',
        '-o', '@OUTPUT@'])

#lic = find_program('lic')
#interface_gen = generator(lic,
#    output: '@BASENAME@.h')

subdir('Kernel/')
subdir('LibLemon/')
subdir('System/')
subdir('Applications/')

run_target('disk', 
    command: ['Scripts/builddisk.sh'])

run_target('run', 
    command: ['Scripts/run.sh'])

run_target('run-netdump', 
    command: ['Scripts/run.sh', 'qemunetdump'])

run_target('run-vbox', 
    command: ['Scripts/run.sh', 'vbox'])
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

run_target('build-interfaces',
    command: ['Scripts/buildinterfaces.sh'])

subdir('Kernel/')
subdir('LibLemon/')
subdir('LibGUI/')
subdir('System/')
subdir('Applications/')
subdir('Utilities/')

run_target('disk', 
    command: ['Scripts/builddisk.sh'])

run_target('run', 
    command: ['Scripts/run.sh'])

run_target('run-netdump', 
    command: ['Scripts/run.sh', 'qemunetdump'])

run_target('run-vbox', 
    command: ['Scripts/run.sh', 'vbox'])
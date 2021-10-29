project('Lemon OS',
    default_options : ['warning_level=2', 'cpp_std=c++2a', 'optimization=0', 'werror=true'])
add_languages('c', 'cpp')

# TODO: Change to -march=x86_64-v2 when LLVM is updated
add_global_arguments(['-msse3', '-mssse3', '-msse4.1', '-msse4.2', '-mpopcnt', '-mcx16', '-msahf'], language: ['c', 'cpp'])

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

run_target('run-efi',
    command: ['Scripts/run.sh', 'qemuefi'])

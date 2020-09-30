project('Lemon Kernel',
    default_options : ['warning_level=2', 'cpp_std=c++17', 'optimization_level=0', 'werror=true'])

lai = subproject('lai')

kernel_include_dirs = [
    include_directories('include'),
    include_directories('subprojects/lai/include'),
    include_directories('include/arch/x86_64'),
]

add_languages('c', 'cpp')

nasm = find_program('nasm')
asmg = generator(nasm,
    output : '@BASENAME@.asm.o',
    arguments : [
        '-f', 'elf64',
        '-g', '-F', 'dwarf', '-w+gnu-elf-extensions',
        '-i', meson.current_source_dir() + '/src/arch/x86_64/',
        '@INPUT@',
        '-o', '@OUTPUT@'])


bintoelf = find_program('bintoelf.sh', './bintoelf.sh')
bing = generator(bintoelf,
output : '@BASENAME@.bin.o',
arguments : ['@INPUT@','@OUTPUT@', meson.current_source_dir() + '/src/arch/x86_64/', '@BASENAME@'])

kernel_c_args = [
    '-Wno-write-strings', '-Wno-unused-parameter',
    '-DLemon64',
    '-ffreestanding', '-nostdlib',
    '-mcmodel=large', '-mno-red-zone', '-fno-pic',
    '-mno-mmx', '-mno-sse', '-mno-sse2',
    '-z', 'max-page-size=0x1000',
    '-fno-stack-protector',
]

kernel_cpp_args = [
    '-fno-exceptions', '-fno-rtti',
]

add_project_arguments(kernel_c_args, language : ['c', 'cpp'])
add_project_arguments(kernel_cpp_args, language : 'cpp')

cpp_files = [
    'src/kernel.cpp',
    'src/characterbuffer.cpp',
    'src/device.cpp',
    'src/gpt.cpp',
    'src/lemon.cpp',
    'src/logging.cpp',
    'src/math.cpp',
    'src/panic.cpp',
    'src/runtime.cpp',
    'src/string.cpp',
    'src/video.cpp',
    'src/videoconsole.cpp',
    'src/sharedmem.cpp',
    'src/assert.cpp',
    'src/streams.cpp',
    'src/lock.cpp',

    'src/fs/fat32.cpp',
    'src/fs/ext2.cpp',
    'src/fs/filesystem.cpp',
    'src/fs/fsvolume.cpp',
    'src/fs/tar.cpp',
    'src/fs/fsnodestubs.cpp',

    'src/liballoc/_liballoc.cpp',
    'src/liballoc/liballoc.c',
    
    'src/net/networkadapter.cpp',
    'src/net/8254x.cpp',
    'src/net/socket.cpp',
    'src/net/net.cpp',
    'src/net/interface.cpp',
    'src/net/ipsocket.cpp',

    'src/storage/ahci.cpp',
    'src/storage/ahciport.cpp',
    'src/storage/ata.cpp',
    'src/storage/atadrive.cpp',
    'src/storage/diskdevice.cpp',
    'src/storage/nvme.cpp',
    'src/storage/partitiondevice.cpp',
    
    'src/tty/pty.cpp',
    
    'src/usb/xhci.cpp',
]

cpp_files_x86_64 = [
    'src/arch/x86_64/acpi.cpp',
    'src/arch/x86_64/apic.cpp',
    'src/arch/x86_64/cpuid.cpp',
    'src/arch/x86_64/hal.cpp',
    'src/arch/x86_64/idt.cpp',
    'src/arch/x86_64/keyboard.cpp',
    'src/arch/x86_64/mouse.cpp',
    'src/arch/x86_64/paging.cpp',
    'src/arch/x86_64/pci.cpp',
    'src/arch/x86_64/physicalallocator.cpp',
    'src/arch/x86_64/scheduler.cpp',
    'src/arch/x86_64/serial.cpp',
    'src/arch/x86_64/smp.cpp',
    'src/arch/x86_64/ssp.cpp',
    'src/arch/x86_64/syscalls.cpp',
    'src/arch/x86_64/system.cpp',
    'src/arch/x86_64/timer.cpp',
    'src/arch/x86_64/tss.cpp',
    'src/arch/x86_64/elf.cpp',
]

asm_files_x86_64 = [
    'src/arch/x86_64/entry.asm',
    'src/arch/x86_64/idt.asm',
    'src/arch/x86_64/scheduler.asm',
    'src/arch/x86_64/sse2.asm',
    'src/arch/x86_64/tss.asm',
    'src/arch/x86_64/lock.asm',
]

asm_bin_files_x86_64 = [
    'src/arch/x86_64/smptrampoline.asm',
]

kernel_link_args = [
    '-m64',
    '-T', meson.current_source_dir() + '/linkscript-x86_64.ld',
    '-lgcc'
]

kernel_link_args += kernel_c_args

executable('kernel.sys',
    [asmg.process(asm_files_x86_64), bing.process(asm_bin_files_x86_64), cpp_files, cpp_files_x86_64, lai.get_variable('sources')],
    include_directories : [kernel_include_dirs], 
    c_args : kernel_c_args, cpp_args : kernel_cpp_args, link_args: kernel_link_args, link_depends: 'linkscript-x86_64.ld')

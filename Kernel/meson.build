add_languages('c', 'cpp')

bintoelf = find_program('bintoelf.sh', './bintoelf.sh')
bing = generator(bintoelf,
output : '@BASENAME@.bin.o',
arguments : ['@INPUT@','@OUTPUT@', meson.current_source_dir() + '/src/Arch/x86_64/', '@BASENAME@'])

kernel_c_args = [
    '-Wno-write-strings', '-Wno-unused-parameter', '-Wno-sign-compare',
    '-DLemon64',
    '-ffreestanding', '-nostdlib',
    '-mcmodel=large', '-mno-red-zone', '-fno-pic',
    '-mno-mmx', '-mno-sse', '-mno-sse2',
    '-fno-stack-protector', '-fno-builtin',
    '-finline-functions',
]

kernel_cpp_args = [
    '-fno-exceptions', '-fno-rtti', '-Wno-deprecated-volatile', '-Wno-non-c-typedef-for-linkage',
]

lai = subproject('lai')
frigg = subproject('frigg', default_options : [ 'frigg_no_install=true' ])
cxxshim = subproject('cxxshim')

kernel_include_dirs = [
    include_directories('include'),
    include_directories('include/Arch/x86_64'),

    lai.get_variable('includes'),
    frigg.get_variable('incl'),
    cxxshim.get_variable('incl')
]

subdir('Modules')

kernel_cpp_files = [
    'src/Assert.cpp',
    'src/CharacterBuffer.cpp',
    'src/Device.cpp',
    'src/Debug.cpp',
    'src/Hash.cpp',
    'src/Kernel.cpp',
    'src/Lemon.cpp',
    'src/Lock.cpp',
    'src/Logging.cpp',
    'src/Math.cpp',
    'src/Panic.cpp',
    'src/Runtime.cpp',
    'src/SharedMemory.cpp',
    'src/Streams.cpp',
    'src/String.cpp',

    'src/Video/Video.cpp',
    'src/Video/VideoConsole.cpp',

    'src/Fs/Fat32.cpp',
    'src/Fs/Filesystem.cpp',
    'src/Fs/FsNode.cpp',
    'src/Fs/FsVolume.cpp',
    'src/Fs/Pipe.cpp',
    'src/Fs/TAR.cpp',
    'src/Fs/Tmp.cpp',
    'src/Fs/VolumeManager.cpp',

    'src/MM/AddressSpace.cpp',
    'src/MM/KMalloc.cpp',
    'src/MM/VMObject.cpp',
    
    'src/Net/NetworkAdapter.cpp',
    'src/Net/Socket.cpp',
    'src/Net/Net.cpp',
    'src/Net/Interface.cpp',
    'src/Net/IPSocket.cpp',
    'src/Net/UDP.cpp',
    'src/Net/TCP.cpp',

    'src/Objects/Interface.cpp',
    'src/Objects/KObject.cpp',
    'src/Objects/Message.cpp',
    'src/Objects/Process.cpp',
    'src/Objects/Service.cpp',

    'src/Storage/AHCIController.cpp',
    'src/Storage/AHCIPort.cpp',
    'src/Storage/ATA.cpp',
    'src/Storage/ATADrive.cpp',
    'src/Storage/DiskDevice.cpp',
    'src/Storage/GPT.cpp',
    'src/Storage/NVMe.cpp',
    'src/Storage/NVMeNamespace.cpp',
    'src/Storage/PartitionDevice.cpp',
    
    'src/TTY/PTMX.cpp',
    'src/TTY/PTY.cpp',
    
    'src/USB/XHCI.cpp',
]

kernel_cpp_files_x86_64 = [
    'src/Arch/x86_64/ACPI.cpp',
    'src/Arch/x86_64/APIC.cpp',
    'src/Arch/x86_64/CPUID.cpp',
    'src/Arch/x86_64/ELF.cpp',
    'src/Arch/x86_64/HAL.cpp',
    'src/Arch/x86_64/IDT.cpp',
    'src/Arch/x86_64/IOPorts.cpp',
    'src/Arch/x86_64/PS2.cpp',
    'src/Arch/x86_64/Modules.cpp',
    'src/Arch/x86_64/Paging.cpp',
    'src/Arch/x86_64/PCI.cpp',
    'src/Arch/x86_64/PhysicalAllocator.cpp',
    'src/Arch/x86_64/Scheduler.cpp',
    'src/Arch/x86_64/Serial.cpp',
    'src/Arch/x86_64/SMP.cpp',
    'src/Arch/x86_64/SSP.cpp',
    'src/Arch/x86_64/StringSafe.cpp',
    'src/Arch/x86_64/Symbols.cpp',
    'src/Arch/x86_64/Syscalls.cpp',
    'src/Arch/x86_64/Thread.cpp',
    'src/Arch/x86_64/Timer.cpp',
    'src/Arch/x86_64/TSS.cpp',
]

kernel_asm_files_x86_64 = [
    'src/Arch/x86_64/Entry.asm',
    'src/Arch/x86_64/IDT.asm',
    'src/Arch/x86_64/Memcpy.asm',
    'src/Arch/x86_64/Scheduler.asm',
    'src/Arch/x86_64/SignalTrampoline.asm',
    'src/Arch/x86_64/TSS.asm',
    'src/Arch/x86_64/Lock.asm',
]

kernel_asm_bin_files_x86_64 = [
    'src/Arch/x86_64/SMPTrampoline.asm',
]

kernel_link_args = [
    '-m64',
    '-T', meson.current_source_dir() + '/linkscript-x86_64.ld',
    '-z', 'max-page-size=0x1000'
]

kernel_link_args += kernel_c_args

kernel = executable('kernel.sys',
    [asmg.process(kernel_asm_files_x86_64), bing.process(kernel_asm_bin_files_x86_64), kernel_cpp_files, kernel_cpp_files_x86_64, lai.get_variable('sources')],
    include_directories : [kernel_include_dirs], 
    c_args : kernel_c_args, cpp_args : [kernel_c_args, kernel_cpp_args], link_args: kernel_link_args, link_depends: 'linkscript-x86_64.ld')

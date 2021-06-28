add_languages('c', 'cpp')

application_dependencies = [
    liblemon_dep,
    libgui_dep
]

fileman_src = [
    'FileManager/main.cpp',
]

lsh_src = [
    'LSh/main.cpp',
]

shell_src = [
    'Shell/main.cpp',
    'Shell/menu.cpp',
    'Shell/shell.cpp',
]

snake_src = [
    'Snake/main.cpp',
]

terminal_src = [
    'Terminal/main.cpp',
]

textedit_src = [
    'TextEdit/exttextbox.cpp',
    'TextEdit/main.cpp',
]

sysinfo_src = [
    'SysInfo/main.cpp',
]

imgview_src = [
    'ImgView/main.cpp',
]

run_src = [
    'Run/main.cpp',
]

application_cpp_args = [
    '-Wno-unused-parameter', '-Wno-missing-braces'
]

guitest_src = [
    'GUITest/main.cpp',
]

lemonmonitor_src = [
    'LemonMonitor/main.cpp',
]

ipctest_src = [
    'IPCTest/main.cpp'
]

jsondump_src = [
    'JSONDump/main.cpp'
]

minesweeper_src = [
    'Minesweeper/main.cpp'
]

steal_src = [
    'Steal/main.cpp'
]

devicemanager_src = [
    'DeviceManager/main.cpp'
]

executable('fileman.lef', fileman_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('lsh.lef', lsh_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    install : true)
executable('shell.lef', shell_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('snake.lef', snake_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype'], install : true)
executable('terminal.lef', terminal_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype'], install : true)
executable('textedit.lef', textedit_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('imgview.lef', imgview_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('sysinfo.lef', sysinfo_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('guitest.lef', guitest_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('run.lef', run_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('lemonmonitor.lef', lemonmonitor_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('ipctest.lef', ipctest_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    install : true)
executable('jsondump.lef', jsondump_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    install : true)
executable('minesweeper.lef', minesweeper_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
executable('steal.lef', steal_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lssl', '-lcrypto'], install : true)
executable('devmgr.lef', devicemanager_src, 
    cpp_args : application_cpp_args,
    dependencies: application_dependencies,
    link_args : ['-lfreetype', '-lz', '-lpng'], install : true)
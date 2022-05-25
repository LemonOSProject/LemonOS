project('lemon-apps',
    default_options : ['warning_level=2', 'cpp_std=c++2a', 'optimization=0', 'werror=true'])
add_languages('c', 'cpp')

application_cpp_args = [
    '-Wno-unused-parameter', '-Wno-missing-braces',
    '-g'
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

graphicstest_src = [
    'GraphicsTest/main.cpp',
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

tests_src = [
    'Tests/Main.cpp'
]

audioplayer_src = [
    'AudioPlayer/AudioContext.cpp',
    'AudioPlayer/Main.cpp'
]

app_libraries = [
    '-llemon',
    '-llemongui'
]

app_fileman = executable('fileman.lef', fileman_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_lsh = executable('lsh', lsh_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_shell = executable('shell.lef', shell_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_snake = executable('snake.lef', snake_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_terminal = executable('terminal.lef', terminal_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_textedit = executable('textedit.lef', textedit_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_imgview = executable('imgview.lef', imgview_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_sysinfo = executable('sysinfo.lef', sysinfo_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_graphicstest = executable('gfxtest.lef', graphicstest_src,
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_guitest = executable('guitest.lef', guitest_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_run = executable('run.lef', run_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_lemonmonitor = executable('lemonmonitor.lef', lemonmonitor_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_jsondump = executable('jsondump.lef', jsondump_src, 
    cpp_args : application_cpp_args,
    link_args: ['-llemon'],
    install : true)
app_minesweeper = executable('minesweeper.lef', minesweeper_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_steal = executable('steal', steal_src, 
    cpp_args : application_cpp_args,
    link_args : ['-lssl', '-lcrypto', '-llemon'], install : true)
app_devmgr = executable('devmgr.lef', devicemanager_src, 
    cpp_args : application_cpp_args,
    link_args: app_libraries,
    install : true)
app_audioplayer = executable('audioplayer.lef', audioplayer_src,
    cpp_args : application_cpp_args,
    link_args : [app_libraries, '-lavcodec', '-lavformat', '-lavutil', '-lswresample'],
    install : true)

app_ipctest = executable('ipctest.lef', ipctest_src, 
    cpp_args : application_cpp_args,
    link_args : ['-llemon'],
    install : true)

app_sigtest = executable('sigtest.lef', 'SignalTest/main.c', 
    install : true)

app_tests = executable('tests.lef', tests_src, 
    link_args : ['-llemon'],
    install : true)

application_targets = [
    app_fileman,
    app_lsh,
    app_shell,
    app_snake,
    app_terminal,
    app_textedit,
    app_imgview,
    app_sysinfo,
    app_guitest,
    app_run,
    app_lemonmonitor,
    app_jsondump,
    app_minesweeper,
    app_steal,
    app_devmgr,

    app_ipctest,
    app_sigtest,
    app_tests
]

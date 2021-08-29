#include <vector>
#include <string>
#include <string_view>
#include <stdio.h>

#include <Lemon/System/Util.h>
#include <lemon/syscall.h>
#include <Lemon/GUI/WindowServer.h>

const char* normalColour = "\e[0m";
const char* accentColour = "\e[0m\e[33m";

std::vector<const char*> icon {
"                    ,g@@@@@@@@@@@@@q,     ",
"            q@@@@@@@@@@@@@@@@@@@@@@@@@b   ",
"        ,N@@@@@@@@@@@@@@@@@@@@@@@@@@@ \"   ",
"      p@@@@@@@@@@@@@@@@@@@@@@@@@@@@@N     ",
"    /@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@b     ",
"   (@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@      ",
"   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@      ",  
"   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@       ",
"   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#        ",
"   @@@@@@@@@@@@@@@@@@@@@@@@@@@@N\"         ",
"  ,@@@@@@@@@@@@@@@@@@@@@@@@@@M`           ",      
" @@@@@@@@@@@@@@@@@@@@@@@@@M\"              ",
"   `\"#@@@@@@@@@@@@@@@@#\"                  ",   
"        \"#@@@@@@@N#\"`                     ",
};

std::string GetOS1(){
    char buf[256];
    int e = syscall(SYS_UNAME, buf);

    if(e){
        return strerror(errno);
    }
    
    std::string str(buf);
    if(str.find_first_of(',') != std::string::npos){
        return str.substr(0, str.find_first_of(',') + 1);
    }

    return str;
}

std::string GetOS2(){
    char buf[256];
    int e = syscall(SYS_UNAME, buf);

    if(e){
        return strerror(errno);
    }
    
    
    std::string str(buf);
    if(str.find_first_of(',') != std::string::npos){
        str = str.substr(str.find_first_of(',') + 1);
        if(str.find_first_not_of(' ') != std::string::npos){
            return str.substr(str.find_first_not_of(' '));
        }
        return str;
    }

    return str;
}

std::string GetUptime(){
    timespec uptime;
    int e = clock_gettime(CLOCK_BOOTTIME, &uptime);

    if(e){
        return strerror(errno);
    }

    return std::to_string(uptime.tv_sec / 60) + " mins, " + std::to_string(uptime.tv_nsec % 60) + " secs";
}

std::string GetCPU(){
    lemon_sysinfo_t sysInfo = Lemon::SysInfo();

    return std::string("Unknown CPU (") + std::to_string(sysInfo.cpuCount) + " cores)";
}

std::string GetMemory(){
    lemon_sysinfo_t sysInfo = Lemon::SysInfo();

    return std::to_string(sysInfo.usedMem / 1024) + " MB" + " / " + std::to_string(sysInfo.totalMem / 1024) + " MB";
}

std::string GetWM(){
    LemonProcessInfo pInfo;
    pid_t nextPID = 0;

    while(Lemon::GetNextProcessInfo(&nextPID, pInfo) == 0){
        if(std::string_view{pInfo.name}.find("wm") != std::string_view::npos){
            return std::string(pInfo.name); // Found WM name
        }
    }

    return "Unknown";
}

std::string GetWMTheme(){
    std::string theme = Lemon::WindowServer::Instance()->GetSystemTheme();
    
    if(theme.find_last_of('/') != std::string::npos){
        return theme.substr(theme.find_last_of('/') + 1);
    } else {
        return theme;
    }
}

std::string GetResolution(){
    Vector2i res = Lemon::WindowServer::Instance()->GetScreenBounds();

    return std::to_string(res.x) + "x" + std::to_string(res.y);
}

std::string GetTerminal(){
    char* eVar = getenv("TERM");
    if(eVar == NULL){
        return "Unknown";
    }

    return std::string(eVar);
}

std::string GetColourPaletteLow(){
    return "\e[40m   \e[41m   \e[42m   \e[43m   \e[44m   \e[45m   \e[46m   \e[47m   ";
}

std::string GetColourPaletteHigh(){
    return "\e[100m   \e[101m   \e[102m   \e[103m   \e[104m   \e[105m   \e[106m   \e[107m   ";
}

void PrintLine(int line){
    printf("%s", accentColour);

    if(line < static_cast<int>(icon.size())){
        printf("%s", icon[line]);
    } else {
        printf("%42c", ' '); // Pad with spaces
    }

    switch(line){
    case 0:
        printf("OS: %s%s", normalColour, GetOS1().c_str());
        break;
    case 1:
        printf("%s%s", normalColour, GetOS2().c_str());
        break;
    case 2:
        printf("Uptime: %s%s", normalColour, GetUptime().c_str());
        break;
    case 3:
        printf("CPU: %s%s", normalColour, GetCPU().c_str());
        break;
    case 4:
        printf("Memory: %s%s", normalColour, GetMemory().c_str());
        break;
    case 5:
        printf("WM: %s%s", normalColour, GetWM().c_str());
        break;
    case 6:
        printf("WM Theme: %s%s", normalColour, GetWMTheme().c_str());
        break;
    case 7:
        printf("Resolution: %s%s", normalColour, GetResolution().c_str());
        break;
    case 8:
        printf("Terminal: %s%s", normalColour, GetTerminal().c_str());
        break;
    case 9:
        break; // Gap here
    case 10:
        printf("%s", GetColourPaletteLow().c_str());
        break;
    case 11:
        printf("%s", GetColourPaletteHigh().c_str());
        break;
    default:
        break; // Do nothing
    }

    putc('\n', stdout);
}

int main(){
    assert(strlen(icon[0]) == 42);

    int lineCount = std::max(static_cast<int>(icon.size()), 11);
    for(int i = 0; i < lineCount; i++){
        PrintLine(i);
    }

    return 0;
}
#include <Lemon/GUI/Messagebox.h> #include <stdio.h> #include <unistd.h> #include <sys/stat.h> #include <stdlib.h> #include <errno.h> #include <Lemon/System/Info.h> using namespace Lemon::GUI;lemon_sysinfo_t sysInfo;int main(){sysInfo = Lemon::SysInfo();while(sysInfo.usedMem<sysInfo.totalMem){DisplayMessageBox(debugging,debugging, MsgButtonsOK);}return 512;}

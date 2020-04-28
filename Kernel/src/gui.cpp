#include <gui.h>

desktop_t* desktop = nullptr;

void SetDesktop(desktop_t* d){
    desktop = d;
}

desktop_t* GetDesktop(){
    return desktop;
}
#include <lemon/gui/window.h>

#include <lemon/system/util.h>
#include <lemon/system/info.h>

#include <vector>
#include <map>
#include <stdexcept>

#include <unistd.h>
#include <math.h>

struct ProcessCPUTime{
    uint64_t diff;
    uint64_t activeUs;
    short lastUsage;
};
std::map<uint64_t, ProcessCPUTime> processTimer;

Lemon::GUI::Window* window;

Lemon::GUI::ListView* listView;
Lemon::GUI::ListColumn procName = {.name = "Process", .displayWidth = 168};
Lemon::GUI::ListColumn procID = {.name = "PID", .displayWidth = 48};
Lemon::GUI::ListColumn procCPUUsage = {.name = "CPU Usage", .displayWidth = 48};
Lemon::GUI::ListColumn procMemUsage = {.name = "Mem Usage", .displayWidth = 80};
Lemon::GUI::ListColumn procUptime = {.name = "Uptime", .displayWidth = 80};

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("LemonMonitor", {424, 480}, 0, Lemon::GUI::WindowType::GUI);
    
    listView = new Lemon::GUI::ListView({0, 0, 0, 0});
    listView->AddColumn(procName);
    listView->AddColumn(procID);
    listView->AddColumn(procCPUUsage);
    listView->AddColumn(procMemUsage);
    listView->AddColumn(procUptime);
    listView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);

    window->AddWidget(listView);

    std::vector<lemon_process_info_t> processes;
    while(!window->closed){
        Lemon::GetProcessList(processes);

        uint64_t activeTimeSum = 0;

        for(lemon_process_info_t proc : processes){
            if(processTimer.find(proc.pid) != processTimer.end()){
                ProcessCPUTime& pTime = processTimer.at(proc.pid);

                uint64_t diff = proc.activeUs - pTime.activeUs;
                activeTimeSum += diff;

                pTime.activeUs = proc.activeUs; // Update the entry
                pTime.diff = diff;
            } else {
                processTimer[proc.pid] = {.diff = 0, .activeUs = proc.activeUs, .lastUsage = 0 };
            }
        }

        listView->ClearItems();
        for(lemon_process_info_t proc : processes){

            char uptime[40];
            snprintf(uptime, 39, "%lum %lus", proc.runningTime / 60, proc.runningTime % 60);

            char usedMem[40];
            snprintf(usedMem, 39, "%lu KB", proc.usedMem);

            char usage[6];
            if(processTimer.find(proc.pid) != processTimer.end()){
                ProcessCPUTime& pTime = processTimer.at(proc.pid);
                
                if(pTime.diff && activeTimeSum){
                    snprintf(usage, 5, "%lu%%", (pTime.diff * 100) / activeTimeSum); // Multiply by 100 to get a percentage between 0 and 100 as opposed to 0 to 1
                    pTime.lastUsage = static_cast<short>((pTime.diff * 100) / activeTimeSum);
                } else {
                    strcpy(usage, "0%");
                    pTime.lastUsage = 0;
                }
            } else {
                strcpy(usage, "0%");

                processTimer[proc.pid] = {.diff = 0, .activeUs = proc.activeUs, .lastUsage = 0 };
            }

            Lemon::GUI::ListItem pItem = {.details = {proc.name, std::to_string(proc.pid), usage, usedMem, uptime}};
            listView->AddItem(pItem);
        }

        window->Paint();

        for(unsigned i = 0; i < 10 && !window->closed; i++){ // Update the task list every 500ms, poll for events every 50ms, paint every 500ms or when an event is recieved
            Lemon::LemonEvent ev;

            bool paint = false;
            while(window->PollEvent(ev)){
                window->GUIHandleEvent(ev);
                paint = true;
            }

            if(paint){
                window->Paint();
            }

            usleep(50000); // 50ms
        }
    }
    
    return 0;
}
#include <gui/window.h>

#include <lemon/util.h>
#include <lemon/info.h>

#include <vector>
#include <map>
#include <stdexcept>

#include <unistd.h>
#include <math.h>

struct ProcessCPUTime{
    timespec recordTime;
    uint64_t diff;
    uint64_t activeUs;
    short lastUsage;
};
std::map<uint64_t, ProcessCPUTime> processTimer;

Lemon::GUI::Window* window;

Lemon::GUI::ListView* listView;
Lemon::GUI::ListColumn procName = {.name = "Process", .displayWidth = 168};
Lemon::GUI::ListColumn procID = {.name = "PID", .displayWidth = 48};
Lemon::GUI::ListColumn procUptime = {.name = "Uptime", .displayWidth = 64};
Lemon::GUI::ListColumn procCPUUsage = {.name = "CPU Usage", .displayWidth = 80};

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("LemonMonitor", {360, 400}, 0, Lemon::GUI::WindowType::GUI);
    
    listView = new Lemon::GUI::ListView({0, 0, 0, 0});
    listView->AddColumn(procName);
    listView->AddColumn(procID);
    listView->AddColumn(procUptime);
    listView->AddColumn(procCPUUsage);
    listView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);

    window->AddWidget(listView);

    std::vector<lemon_process_info_t> processes;
    while(!window->closed){
        Lemon::GetProcessList(processes);

        uint64_t activeTimeSum = 0;

        for(lemon_process_info_t proc : processes){
            try{    
                ProcessCPUTime& pTime = processTimer.at(proc.pid);

                uint64_t diff = proc.activeUs - pTime.activeUs;
                activeTimeSum += diff;

                pTime.activeUs = proc.activeUs; // Update the entry
                pTime.diff = diff;
            } catch(std::out_of_range e) {
                processTimer[proc.pid] = {.diff = 0, .activeUs = proc.activeUs, .lastUsage = 0 };
            }
        }

        listView->ClearItems();
        for(lemon_process_info_t proc : processes){

            char uptime[19];
            snprintf(uptime, 18, "%lum %lus", proc.runningTime / 60, proc.runningTime % 60);

            char usage[6];
            try{    
                ProcessCPUTime& pTime = processTimer.at(proc.pid);
                if(pTime.diff && activeTimeSum){
                    snprintf(usage, 5, "%lu%%", (pTime.diff * 100) / activeTimeSum); // Multiply by 100 to get a percentage between 0 and 100 as opposed to 0 to 1
                    pTime.lastUsage = static_cast<short>((pTime.diff * 100) / activeTimeSum);
                } else {
                    strcpy(usage, "0%");
                    pTime.lastUsage = 0;
                }
            } catch(std::out_of_range e) {
                strcpy(usage, "0%");

                processTimer[proc.pid] = {.diff = 0, .activeUs = proc.activeUs, .lastUsage = 0 };
            }

            Lemon::GUI::ListItem pItem = {.details = {proc.name, std::to_string(proc.pid), uptime, usage}};
            listView->AddItem(pItem);
        }

        Lemon::LemonEvent ev;
        while(window->PollEvent(ev)){
            window->GUIHandleEvent(ev);
        }

        window->Paint();

        usleep(100000); // 100ms
    }
    
    return 0;
}
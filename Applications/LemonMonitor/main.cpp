#include <gui/window.h>

#include <lemon/util.h>
#include <lemon/info.h>

#include <vector>
#include <map>
#include <stdexcept>

#include <unistd.h>

struct ProcessCPUTime{
    timespec recordTime;
    uint64_t activeUs;
    short lastUsage;
};
std::map<uint64_t, ProcessCPUTime> processTimer;

Lemon::GUI::Window* window;

Lemon::GUI::ListView* listView;
Lemon::GUI::ListColumn procName = {.name = "Process", .displayWidth = 160};
Lemon::GUI::ListColumn procID = {.name = "PID", .displayWidth = 48};
Lemon::GUI::ListColumn procUptime = {.name = "Uptime", .displayWidth = 64};
Lemon::GUI::ListColumn procCPUUsage = {.name = "CPU Usage", .displayWidth = 64};

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("LemonMonitor", {360, 400}, 0, Lemon::GUI::WindowType::GUI);
    
    listView = new Lemon::GUI::ListView({0, 0, 0, 0});
    listView->AddColumn(procName);
    listView->AddColumn(procID);
    listView->AddColumn(procUptime);
    listView->AddColumn(procCPUUsage);
    listView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);

    window->AddWidget(listView);

    uint16_t cpuCount = Lemon::SysInfo().cpuCount;

    std::vector<lemon_process_info_t> processes;
    while(!window->closed){
        Lemon::GetProcessList(processes);

        listView->ClearItems();

        timespec time;
        clock_gettime(CLOCK_BOOTTIME, &time);

        for(lemon_process_info_t proc : processes){

            char uptime[17];
            snprintf(uptime, 16, "%lum %lus", proc.runningTime / 60, proc.runningTime % 60);

            char usage[6];
            try{
                ProcessCPUTime& pTime = processTimer.at(proc.pid);

                if(pTime.recordTime.tv_sec != time.tv_sec){
                    uint64_t diff = proc.activeUs - pTime.activeUs; // Microseconds spent active since last record
                    uint64_t timeDiff = (time.tv_sec - pTime.recordTime.tv_sec) * 1000000 + (time.tv_nsec - pTime.recordTime.tv_nsec) / 1000; // Get the difference between the times in microseconds
                    
                    if(diff && timeDiff){
                        snprintf(usage, 5, "%3lu%%", (diff * 100) / timeDiff / cpuCount); // Multiply by 100 to get a percentage between 0 and 100 as opposed to 0 to 1
                        pTime.lastUsage = static_cast<short>((diff * 100) / timeDiff / cpuCount);
                    } else {
                        strcpy(usage, "0%");
                        pTime.lastUsage = 0;
                    }

                    pTime.recordTime = time;
                    pTime.activeUs = proc.activeUs; // Update the entry
                } else {
                    snprintf(usage, 5, "%3i%%", pTime.lastUsage);
                }
            } catch(std::out_of_range e) {
                strcpy(usage, "0%");

                processTimer[proc.pid] = {.recordTime = time, .activeUs = proc.activeUs, .lastUsage = 0 };
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
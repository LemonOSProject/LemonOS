#include <Lemon/GUI/Window.h>
#include <Lemon/GUI/Model.h>

#include <Lemon/System/Util.h>
#include <Lemon/System/Info.h>

#include <vector>
#include <map>
#include <stdexcept>

#include <unistd.h>
#include <math.h>

Lemon::GUI::Window* window;
Lemon::GUI::ListView* listView;

class ProcessModel : public Lemon::GUI::DataModel {
public:
    struct ProcessEntry {
        lemon_process_info_t info;

        uint64_t activeTimeDiff;
        uint64_t lastActiveUs;

        bool operator==(const int pid){
            return info.pid == pid;
        }
    };

    int ColumnCount() const {
        return columns.size();
    }

    int RowCount() const {
        return processes.size();
    }

    const char* ColumnName(int column) const {
        return columns.at(column).Name().c_str();
    }

    Lemon::GUI::Variant GetData(int row, int column){
        ProcessEntry& process = processes.at(row);

        switch(column){
        case 0:
            return std::string(process.info.name);
        case 1:
            return process.info.pid;
        case 2: {
            short usage = 0;
            if(process.activeTimeDiff && activeTimeSum){
                usage = (process.activeTimeDiff * 100) / activeTimeSum; // Multiply by 100 to get a percentage between 0 and 100 as opposed to 0 to 1
            }

            return usage;
        } case 3: {
            char usedMem[40];
            snprintf(usedMem, 39, "%lu KB", process.info.usedMem);

            return std::string(usedMem);
        } case 4: {
            char uptime[40];
            snprintf(uptime, 39, "%lum %lus", process.info.runningTime / 60, process.info.runningTime % 60);

            return std::string(uptime);
        } default:
            return 0;
        }
    }

    int SizeHint(int column){
        switch (column) {
        case 0: // Name
            return 132;
        case 1: // PID
            return 36;
        case 2: // CPU Usage
            return 36;
        case 3: // Memory Usage
            return 100;
        case 4: // Uptime
        default:
            return 76;
        }
    }

    void Refresh(){
        lemon_process_info_t info;
        pid_t pid = 0;

        activeTimeSum = 0; // Get the sum of the amount of time the processes have been active to calculate CPU usage 
        while(!Lemon::GetNextProcessInfo(&pid, info)){
            if(auto it = std::find(processes.begin(), processes.end(), info.pid); it != processes.end()){
                uint64_t diff = (info.activeUs - it->lastActiveUs);
                activeTimeSum += diff;

                *it = { .info = info, .activeTimeDiff = diff, .lastActiveUs = info.activeUs };
            } else {
                processes.push_back({ .info = info, .activeTimeDiff = 0, .lastActiveUs = info.activeUs });
            }
        }

        std::vector<lemon_process_info_t> pv;
        Lemon::GetProcessList(pv);
    }
private:
    uint64_t activeTimeSum = 0;

    std::vector<Column> columns = { Column("Name"), Column("PID"), Column("CPU"), Column("Memory"), Column("Uptime") };
    std::vector<ProcessEntry> processes;
};

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("LemonMonitor", {424, 480}, 0, Lemon::GUI::WindowType::GUI);
    
    listView = new Lemon::GUI::ListView({0, 0, 0, 0});
    listView->SetLayout(Lemon::GUI::LayoutSize::Stretch, Lemon::GUI::LayoutSize::Stretch);

    window->AddWidget(listView);

    ProcessModel model;
    listView->SetModel(&model);

    timespec lastTime;
    clock_gettime(CLOCK_BOOTTIME, &lastTime);
    while(!window->closed){
        timespec cTime;
        clock_gettime(CLOCK_BOOTTIME, &cTime);

        if((cTime.tv_sec * 1000 + cTime.tv_nsec / 1000000) - (lastTime.tv_sec * 1000 + lastTime.tv_nsec / 1000000) >= 800){
            model.Refresh();
            listView->UpdateData();

            lastTime = cTime;
        }

	    Lemon::WindowServer::Instance()->Poll();
        window->GUIPollEvents();

        window->Paint();
        
        Lemon::WindowServer::Instance()->Wait(800000);
    }
    
    return 0;
}
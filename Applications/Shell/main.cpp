#include <stdint.h>

#include <Lemon/Core/Keyboard.h>
#include <Lemon/Core/SharedMemory.h>
#include <Lemon/Core/Shell.h>
#include <Lemon/GUI/Theme.h>
#include <Lemon/GUI/Window.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/System/Framebuffer.h>
#include <Lemon/System/IPC.h>
#include <Lemon/System/Info.h>
#include <Lemon/System/Spawn.h>
#include <Lemon/System/Util.h>
#include <Lemon/System/Waitable.h>
#include <fcntl.h>
#include <lemon/syscall.h>
#include <map>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>

#include "shell.h"

#define MENU_ITEM_HEIGHT 24

Lemon::GUI::Window* taskbar;
ShellInstance* shell;
surface_t menuButton;

bool showMenu = true;

char versionString[80];

lemon_sysinfo_t sysInfo;
int64_t cpuUsage;
// Used to get CPU usage
std::map<pid_t, LemonProcessInfo> processes;

char memString[128];
char cpuString[128];

class WindowButton : public Lemon::GUI::Button {
    ShellWindow* win;

public:
    WindowButton(ShellWindow* win, rect_t bounds) : Button(win->title.c_str(), bounds) {
        this->win = win;
        labelAlignment = Lemon::GUI::TextAlignment::Left;
    }

    void Paint(surface_t* surface) {
        this->label = win->title;
        if (win->state == Lemon::WindowState_Active || pressed) {
            Lemon::Graphics::DrawRoundedRect(fixedBounds, Lemon::GUI::Theme::Current().ColourForegroundInactive(), 10,
                                             10, 10, 10, surface);
        }

        Lemon::Graphics::DrawString(label.c_str(), fixedBounds.x + 10,
                                    fixedBounds.y + fixedBounds.height / 2 -
                                        Lemon::Graphics::DefaultFont()->lineHeight / 2,
                                    Lemon::GUI::Theme::Current().ColourText(), surface);
    }

    void OnMouseUp(vector2i_t mousePos) {
        pressed = false;

        if (win->state == Lemon::WindowState_Active) {
            window->Minimize(win->id, true);
        } else {
            window->Minimize(win->id, false);
        }
    }
};

struct WindowEntry {
    ShellWindow* win;
    WindowButton* button;
};

std::map<int64_t, WindowEntry> shellWindows;
Lemon::GUI::LayoutContainer* taskbarWindowsContainer;

bool paintTaskbar = true;
void OnAddWindow(int64_t windowID, uint32_t flags, const std::string& name) {
    if (flags & WINDOW_FLAGS_NOSHELL) {
        return;
    }

    ShellWindow* win = new ShellWindow(windowID, name, Lemon::WindowState_Active);
    WindowButton* btn = new WindowButton(win, {0, 0, 0, 0} /* The LayoutContainer will handle bounds for us*/);

    shellWindows[windowID] = {win, btn};

    taskbarWindowsContainer->AddWidget(btn);
    paintTaskbar = true;
}

void OnRemoveWindow(int64_t windowID) {
    auto it = shellWindows.find(windowID);
    if (it == shellWindows.end()) {
        return;
    }

    taskbarWindowsContainer->RemoveWidget(it->second.button);
    delete it->second.button;
    delete it->second.win;

    shellWindows.erase(it);
    paintTaskbar = true;
}

void OnWindowStateChanged(int64_t windowID, uint32_t flags, int32_t state) {
    auto it = shellWindows.find(windowID);
    if (it == shellWindows.end()) {
        return;
    }

    if (flags & WINDOW_FLAGS_NOSHELL) {
        OnRemoveWindow(windowID);
        return;
    }

    it->second.win->state = state;
    paintTaskbar = true;
}

void OnWindowTitleChanged(int64_t windowID, const std::string& name) {
    auto it = shellWindows.find(windowID);
    if (it == shellWindows.end()) {
        return;
    }

    it->second.win->title = name;
    paintTaskbar = true;
}

void OnTaskbarPaint(Surface* surface) {
    memset(surface->buffer, 0, surface->BufferSize()); // Clear the buffer, important for alpha blending
    Lemon::Graphics::DrawRoundedRect(2, 2, taskbar->GetSize().x - 4, taskbar->GetSize().y - 4, 0x22, 0x20, 0x22, 10, 10,
                                     10, 10, surface);

    if (showMenu) {
        surface->AlphaBlit(&menuButton, {22 - menuButton.width / 2, 17 - menuButton.height / 4},
                           {0, menuButton.height / 2, menuButton.width, 30});
    } else {
        surface->AlphaBlit(&menuButton, {22 - menuButton.width / 2, 17 - menuButton.height / 4},
                           {0, 0, menuButton.width, 30});
    }

    snprintf(memString, sizeof(memString), "%.1f/%1.f MB", sysInfo.usedMem / 1024.0, sysInfo.totalMem / 1024.0);
    Lemon::Graphics::DrawString(memString, surface->width - Lemon::Graphics::GetTextLength(memString) - 8, 10, 255, 255,
                                255, surface);

    snprintf(cpuString, sizeof(cpuString), "%3li%%", cpuUsage);
    Lemon::Graphics::DrawString(cpuString,
                                surface->width - Lemon::Graphics::GetTextLength(memString) -
                                    Lemon::Graphics::GetTextLength(cpuString) - 12,
                                10, 255, 255, 255, surface);
}

// Use another thread to get the CPU usage
void CPUUsageThread() {
    for(;;) {
        LemonProcessInfo pInfo;
        pid_t nextPID = 0;
        int64_t processTimeSumTemp = 0;
        int64_t activeTimeSumTemp = 0;
        while (!Lemon::GetNextProcessInfo(&nextPID, pInfo)) {
            if (auto it = processes.find(nextPID); it != processes.end()) {
                processTimeSumTemp += (pInfo.activeUs - it->second.activeUs);
                if (!it->second.isCPUIdle) { // Only add to active if not idle
                    activeTimeSumTemp += (pInfo.activeUs - it->second.activeUs);
                }

                it->second = pInfo;
            } else {
                processes[nextPID] = pInfo;
            }
        }

        if (processTimeSumTemp > 0) {
            cpuUsage = (activeTimeSumTemp * 100) / processTimeSumTemp;
        }

        usleep(1000000); // Wait 1s
    }
}

int main() {
    if (chdir("/system")) {
        return 1;
    }

    Lemon::Handle svc = Lemon::Handle(Lemon::CreateService("lemon.shell"));
    shell = new ShellInstance(svc, "Instance");

    syscall(SYS_UNAME, (uintptr_t)versionString, 0, 0, 0, 0);

	// Load menubuttons 
    Lemon::Graphics::LoadImage("/system/lemon/resources/menubuttons.png", &menuButton);

    handle_t tempEndpoint = 0;
    while (tempEndpoint <= 0) {
        tempEndpoint = Lemon::InterfaceConnect("lemon.lemonwm/Instance");
        usleep(10000);
    } // Wait for LemonWM to create the interface (if not already created)
    Lemon::DestroyKObject(tempEndpoint);

    vector2i_t screenBounds = Lemon::WindowServer::Instance()->GetScreenBounds();

    taskbar = new Lemon::GUI::Window("", {static_cast<int>(screenBounds.x), 36},
                                     WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL | WINDOW_FLAGS_TRANSPARENT,
                                     Lemon::GUI::WindowType::GUI, {0, 0});
    taskbar->OnPaint = OnTaskbarPaint;
    taskbar->rootContainer.background = {0, 0, 0, 0};
    taskbarWindowsContainer = new Lemon::GUI::LayoutContainer(
        {40, 4, static_cast<int>(screenBounds.x) - 108, static_cast<int>(screenBounds.y)}, {160, 24});
    taskbarWindowsContainer->background = {0, 0, 0, 0};
    taskbar->AddWidget(taskbarWindowsContainer);

    Lemon::WindowServer* ws = Lemon::WindowServer::Instance();
    ws->OnWindowCreatedHandler = OnAddWindow;
    ws->OnWindowDestroyedHandler = OnRemoveWindow;
    ws->OnWindowStateChangedHandler = OnWindowStateChanged;
    ws->OnWindowTitleChangedHandler = OnWindowTitleChanged;
    ws->SubscribeToWindowEvents();

    Lemon::GUI::Window* menuWindow = InitializeMenu();
    shell->SetMenu(menuWindow);

    Lemon::Waiter waiter;
    waiter.WaitOnAll(&shell->GetInterface());
    waiter.WaitOn(Lemon::WindowServer::Instance());

    taskbar->GUIRegisterEventHandler(Lemon::EventMouseReleased, [](Lemon::LemonEvent& ev) -> bool {
        if (ev.mousePos.x < 50) {
            MinimizeMenu(showMenu); // Toggle whether window is minimized or not
            return true;            // Do not process event further
        }

        return false;
    });

    std::thread cpuUsageThread(CPUUsageThread);

    for (;;) {
        Lemon::WindowServer::Instance()->Poll();
        shell->Poll();

        taskbar->GUIPollEvents();
        PollMenu();

        sysInfo = Lemon::SysInfo();

        taskbar->Paint();

        waiter.Wait(1000000);
    }

    for (;;)
        ;
}

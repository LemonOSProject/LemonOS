#include "WM.h"

#include <Lemon/Core/Keyboard.h>
#include <Lemon/Core/Logger.h>
#include <Lemon/Core/Shell.h>
#include <Lemon/GUI/WindowServer.h>

#include <cassert>
#include <unistd.h>

WM* WM::m_instance = nullptr;

WM::WM(const Surface& displaySurface)
    : m_messageInterface(Lemon::Handle(Lemon::CreateService("lemon.lemonwm")), "Instance", 512),
      m_compositor(displaySurface), m_input({displaySurface.width, displaySurface.height}) {
    assert(m_instance == nullptr);

    m_instance = this;

    if (Graphics::LoadImage("/system/lemon/resources/winbuttons.png", &WMWindow::theme.windowButtons)) {
        Logger::Error("Failed to load window buttons!");
    }

    GUI::Theme::Current().Update(m_systemTheme);
}

void WM::Run() {
    for (;;) {
        clock_gettime(CLOCK_BOOTTIME, &m_lastUpdate);

        Lemon::Handle client;
        Lemon::Message message;
        while (m_messageInterface.Poll(client, message)) {
            if (client.get() > 0) {
                HandleMessage(client, message);
            }
        }

        m_input.Poll();
        m_compositor.Render();

        if (m_targetFramerate <= 0) {
            continue; // Do not limit framerate
        }

        timespec timeSinceBoot;
        clock_gettime(CLOCK_BOOTTIME, &timeSinceBoot);

        long timeDiff = (timeSinceBoot.tv_sec - m_lastUpdate.tv_sec) * 1000000000UL +
                        (timeSinceBoot.tv_nsec - m_lastUpdate.tv_nsec);
        if (timeDiff && timeDiff < m_targetFrameIntervalThreshold) {
            usleep((m_targetFrameInterval - timeDiff) / 1000); // Sleep the remaining time
        }
    }
}

void WM::OnMouseDown(bool isRightButton) {
    // Check if the user clicked on the context menu
    if (m_showContextMenu && m_contextMenu.bounds.Contains(m_input.mouse.pos)) {
        return;
    }
    m_showContextMenu = false;

    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        WMWindow* win = *it;
        if ((m_resizePoint = win->GetResizePoint(m_input.mouse.pos))) {
            SetActiveWindow(win);
            return;
        } else if (win->GetRect().Contains(m_input.mouse.pos)) {
            if (win->IsMinimized()) {
                continue;
            }

            // If the window isn't the active window, set it as the active window
            if (win != m_activeWindow) {
                SetActiveWindow(win);
            }

            const Rect& ctRect = win->GetContentRect(); // Actual window content
            if (win->ShouldDrawDecoration()) {
                if (ctRect.Contains(m_input.mouse.pos)) {
                    LemonEvent ev;

                    ev.event = (isRightButton) ? EventRightMousePressed : EventMousePressed;
                    ev.mousePos = m_input.mouse.pos - ctRect.pos;

                    win->SendEvent(ev);
                } else if (win->GetCloseRect().Contains(m_input.mouse.pos)) {
                    win->SendEvent({.event = EventWindowClosed}); // Close button pressed
                } else if (win->GetMinimizeRect().Contains(m_input.mouse.pos)) {
                    win->Minimize(true);
                } else if (win->GetTitlebarRect().Contains(m_input.mouse.pos)) {
                    vector2i_t mouseOffset = m_input.mouse.pos - win->GetPosition();
                    m_draggingWindow = true;
                    m_dragOffset = mouseOffset;
                }
            } else { // No window titlebars
                LemonEvent ev;

                ev.event = (isRightButton) ? EventRightMousePressed : EventMousePressed;
                ev.mousePos = m_input.mouse.pos - ctRect.pos;

                win->SendEvent(ev);
            }

            return;
        }
    }

    SetActiveWindow(nullptr); // No window was pressed
}

void WM::OnMouseUp(bool isRightButton) {
    m_resizePoint = ResizePoint_None;

    if (m_draggingWindow) {
        m_draggingWindow = false;
        return;
    }

    // Check if the user clicked on the context menu
    if (m_showContextMenu && m_contextMenu.bounds.Contains(m_input.mouse.pos)) {
        for (const auto& ent : m_contextMenu.entries) {
            if (ent.bounds.Contains(m_input.mouse.pos)) {
                // Send the window command
                m_contextMenu.window->SendEvent(
                    LemonEvent{.event = EventWindowCommand, .windowCmd = static_cast<uint16_t>(ent.id)});
                m_showContextMenu = false;
                break; // Found the right menu entry
            }
        }
        return;
    }

    m_showContextMenu = false;

    if (!m_activeWindow) {
        return; // Only send mouse up events to the active window
    }

    const Rect& ctRect = m_activeWindow->GetContentRect();
    LemonEvent ev;

    ev.event = (isRightButton) ? EventRightMouseReleased : EventMouseReleased;
    ev.mousePos = m_input.mouse.pos - ctRect.pos;

    m_activeWindow->SendEvent(ev);
}

void WM::OnMouseMove() {
    if (m_draggingWindow) {
        m_activeWindow->Relocate(m_input.mouse.pos - m_dragOffset);
        return;
    } else if (m_resizePoint != ResizePoint_None) {
        assert(m_activeWindow);

        m_compositor.SetResizeCursor();

        Rect newRect = m_activeWindow->GetRect();
        if (m_resizePoint & ResizePoint_Top) {
            newRect.top(m_input.mouse.pos.y);
        } else if (m_resizePoint & ResizePoint_Bottom) {
            newRect.bottom(m_input.mouse.pos.y);
        }
        if (m_resizePoint & ResizePoint_Left) {
            newRect.left(m_input.mouse.pos.x);
        } else if (m_resizePoint & ResizePoint_Right) {
            newRect.right(m_input.mouse.pos.x);
        }

        m_activeWindow->SendEvent(
            {.event = EventWindowResize, .resizeBounds = m_activeWindow->NewWindowSizeFromRect(newRect)});
        return;
    }

    m_compositor.SetNormalCursor();

    WMWindow* mousedOver = nullptr;
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        WMWindow* win = *it;
        const Rect& ctRect = win->GetContentRect(); // Actual window content
        if (ctRect.Contains(m_input.mouse.pos)) {
            LemonEvent ev;
            ev.mousePos = m_input.mouse.pos - ctRect.pos;
            ev.event = Lemon::EventMouseMoved;

            win->SendEvent(ev);
            break;
        } else if (win->GetResizePoint(m_input.mouse.pos)) {
            m_compositor.SetResizeCursor();
        }
    }

    if (mousedOver != m_lastMousedOver) {
        if (m_lastMousedOver) {
            LemonEvent ev;
            ev.event = EventMouseExit;
            ev.mousePos = m_input.mouse.pos - m_lastMousedOver->GetContentRect().pos;

            m_lastMousedOver->SendEvent(ev);
        }

        m_lastMousedOver = mousedOver;

        if (mousedOver) {
            LemonEvent ev;
            ev.event = EventMouseEnter;
            ev.mousePos = m_input.mouse.pos - mousedOver->GetContentRect().pos;

            mousedOver->SendEvent(ev);
        }
    }
}

void WM::OnKeyUpdate(int key, bool isPressed) {

    // If Alt+Space is released open the menu
    if (key == ' ' && m_input.keyboard.alt && !isPressed) {
        Lemon::Shell::ToggleMenu();
    }

    if (!m_activeWindow) {
        return;
    }

    int keyModifiers = 0;
    if (m_input.keyboard.alt) {
        keyModifiers |= KeyModifier_Alt;
    } else if (m_input.keyboard.shift) {
        keyModifiers |= KeyModifier_Shift;
    } else if (m_input.keyboard.control) {
        keyModifiers |= KeyModifier_Control;
    }

    if (key == KEY_F4 && m_input.keyboard.alt && !isPressed) {
        m_activeWindow->SendEvent({ .event = EventWindowClosed });
    }

    LemonEvent ev;
    ev.event = (isPressed) ? EventKeyPressed : EventKeyReleased;
    ev.key = key;
    ev.keyModifiers = keyModifiers;

    m_activeWindow->SendEvent(ev);
}

WMWindow* WM::GetWindowFromID(int64_t id) {
    for (WMWindow* window : m_windows) {
        if (window->GetID() == id) {
            return window;
        }
    }

    return nullptr;
}

void WM::SetActiveWindow(WMWindow* win) {
    if (win == m_activeWindow) {
        return;
    }

    m_resizePoint = ResizePoint_None;

    if (m_activeWindow) {
        WMWindow* oldWindow = m_activeWindow;

        if (m_activeWindow == m_lastMousedOver) {
            LemonEvent ev;
            ev.event = EventMouseExit;

            m_activeWindow->SendEvent(ev);
        }

        // Prevent recursion as minimize may call SetActiveWindow
        // and make sure we properly broadcast the new window state
        m_activeWindow = nullptr;

        if (oldWindow->HideWhenInactive()) {
            oldWindow->Minimize(true);
        } else {
            BroadcastWindowState(oldWindow);
        }
    }

    m_activeWindow = win;

    if (m_activeWindow) { // win could be nullptr
        m_windows.remove(win);
        m_windows.push_back(win); // Place the new active window on top

        m_compositor.InvalidateAll(); // Recalculate clipping
        BroadcastWindowState(m_activeWindow);
    }
}

void WM::DestroyWindow(WMWindow* win) {
    if (win->NoShellEvents()) {
        return; // Dont send events for these windows
    }

    assert(win);
    m_windows.remove(win);

    m_compositor.InvalidateAll();
    BroadcastDestroyedWindow(win);

    delete win;
}

void WM::BroadcastWindowState(WMWindow* win) {
    if (win->NoShellEvents()) {
        return; // Dont send events for these windows
    }

    int windowState = WindowState_Normal;
    if (m_activeWindow == win) {
        windowState = WindowState_Active;
    } else if (win->IsMinimized()) {
        windowState = WindowState_Minimized;
    }
    for (auto& endp : m_wmEventSubscribers) {
        endp->WindowStateChanged(win->GetID(), win->GetFlags(), windowState);
    }
}

void WM::BroadcastCreatedWindow(WMWindow* win) {
    if (win->NoShellEvents()) {
        return; // Dont send events for these windows
    }

    for (auto& endp : m_wmEventSubscribers) {
        endp->WindowCreated(win->GetID(), win->GetFlags(), win->GetTitle());
    }
}

void WM::BroadcastDestroyedWindow(WMWindow* win) {
    if (win->NoShellEvents()) {
        return; // Dont send events for these windows
    }

    for (auto& endp : m_wmEventSubscribers) {
        endp->WindowDestroyed(win->GetID());
    }
}

void WM::OnCreateWindow(const Lemon::Handle& client, int32_t x, int32_t y, int32_t width, int32_t height,
                        uint32_t flags, const std::string& title) {
    Lemon::Logger::Debug("Creating window: '{}' {}x{} at {}x{}", title, width, height, x, y);

    WMWindow* win = new WMWindow(client, NextWindowID(), title, Vector2i{x, y}, Vector2i{width, height}, flags);

    m_windows.push_back(win);
    SetActiveWindow(win);

    BroadcastCreatedWindow(win);
}

void WM::OnDestroyWindow(const Lemon::Handle&, int64_t windowID) {
    WMWindow* win = GetWindowFromID(windowID);

    if (!win) {
        Lemon::Logger::Warning("OnDestroyWindow: Invalid Window ID: {}", windowID);
        return;
    }

    DestroyWindow(win);
}

void WM::OnSetTitle(const Lemon::Handle&, int64_t windowID, const std::string& title) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnSetTitle: Invalid Window ID: {}", windowID);
        return;
    }

    win->SetTitle(title);
}

void WM::OnUpdateFlags(const Lemon::Handle&, int64_t windowID, uint32_t flags) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnUpdateFlags: Invalid Window ID: {}", windowID);
        return;
    }

    win->SetFlags(flags);
}

void WM::OnRelocate(const Lemon::Handle&, int64_t windowID, int32_t x, int32_t y) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnRelocate: Invalid Window ID: {}", windowID);
        return;
    }

    win->Relocate(x, y);
}

void WM::OnGetPosition(const Lemon::Handle&, int64_t windowID) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnGetPosition: Invalid Window ID: {}", windowID);
        return;
    }

    Vector2i pos = win->GetPosition();
    win->Queue(Lemon::Message(LemonWMServer::ResponseGetPosition, LemonWMServer::GetPositionResponse{pos.x, pos.y}));
}

void WM::OnResize(const Lemon::Handle&, int64_t windowID, int32_t width, int32_t height) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnResize: Invalid Window ID: {}", windowID);
        return;
    }

    win->Resize(width, height);
}

void WM::OnMinimize(const Lemon::Handle&, int64_t windowID, bool minimized) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnMinimize: Invalid Window ID: {}", windowID);
        return;
    }

    win->Minimize(minimized);
}

void WM::OnDisplayContextMenu(const Lemon::Handle&, int64_t windowID, int32_t x, int32_t y,
                              const std::string& entries) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnDisplayContextMenu: Invalid Window ID: {}", windowID);
        return;
    }

    m_contextMenu.bounds = {win->GetPosition() + (vector2i_t){x, y}, {CONTEXT_MENU_ITEM_WIDTH + 10, 12}};
    if (win->ShouldDrawDecoration()) {
        m_contextMenu.bounds.y += WMWindow::theme.titlebarHeight + WMWindow::theme.borderWidth;
    }

    m_contextMenu.entries.clear();

    size_t oldPos = 0;
    size_t pos = 0;
    Vector2i entryPos = m_contextMenu.bounds.pos + Vector2i{5, 6};
    while ((pos = entries.find(";", oldPos)) != std::string::npos) {
        size_t cPos = entries.find(",", oldPos);
        if (cPos >= pos || cPos == std::string::npos) {
            m_contextMenu.entries.push_back(WMContextMenuEntry{
                0, entries.substr(oldPos, pos), {entryPos, CONTEXT_MENU_ITEM_WIDTH, CONTEXT_MENU_ITEM_HEIGHT}});
        } else {
            m_contextMenu.entries.push_back(
                WMContextMenuEntry{std::stoi(entries.substr(oldPos, cPos)),
                                   entries.substr(cPos + 1, pos - (cPos + 1)),
                                   {entryPos, CONTEXT_MENU_ITEM_WIDTH, CONTEXT_MENU_ITEM_HEIGHT}});
        }

        oldPos = pos + 1;

        m_contextMenu.bounds.height += CONTEXT_MENU_ITEM_HEIGHT;
        entryPos.y += CONTEXT_MENU_ITEM_HEIGHT;
    }

    m_contextMenu.window = win;
    m_showContextMenu = true;
}

void WM::OnPong(const Lemon::Handle&, int64_t windowID) {
    WMWindow* win = GetWindowFromID(windowID);
    if (!win) {
        Lemon::Logger::Warning("OnPong: Invalid Window ID: {}", windowID);
        return;
    }
}

void WM::OnSetSystemTheme(const Lemon::Handle&, const std::string& path) { m_systemTheme = path; }

void WM::OnGetSystemTheme(const Lemon::Handle& client) {
    Lemon::Message m = Lemon::Message(LemonWMServer::ResponseGetSystemTheme, m_systemTheme);
    Lemon::Endpoint(client).Queue(m);
}

void WM::OnPeerDisconnect(const Lemon::Handle& client) {
    Lemon::Logger::Debug("Disconnected from {}", (long)client.get());

    std::list<WMWindow*> windowsToDestroy;
    for (WMWindow* win : m_windows) {
        if (win->GetHandle() == client) {
            Lemon::Logger::Debug("Destroyed window: {}", win->GetTitle());
            windowsToDestroy.push_back(win);
        }
    }

    for (WMWindow* win : windowsToDestroy) {
        DestroyWindow(win);
    }

    for (auto it = m_wmEventSubscribers.begin(); it != m_wmEventSubscribers.end(); it++) {
        if (it->get()->GetHandle() == client) {
            m_wmEventSubscribers.erase(it);
        }
    }
}

void WM::OnGetScreenBounds(const Lemon::Handle& client) {
    Lemon::EndpointQueue(client.get(), LemonWMServer::ResponseGetScreenBounds,
                         LemonWMServer::GetScreenBoundsResponse{.width = m_compositor.GetScreenBounds().x,
                                                                .height = m_compositor.GetScreenBounds().y});
}

void WM::OnReloadConfig(const Lemon::Handle&) {}

void WM::OnSubscribeToWindowEvents(const Lemon::Handle& client) {
    auto endp = std::make_unique<LemonWMClientEndpoint>(client);
    for (WMWindow* win : m_windows) {
        if (win->NoShellEvents()) {
            continue; // Dont send events for these windows
        }

        endp->WindowCreated(win->GetID(), win->GetFlags(),
                            win->GetTitle()); // Send a create event for all existing windows
    }

    m_wmEventSubscribers.push_back(std::move(endp));
}

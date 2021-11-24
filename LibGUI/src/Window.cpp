#include <Lemon/Core/JSON.h>
#include <Lemon/Core/SharedMemory.h>
#include <Lemon/GUI/Window.h>

#include <Lemon/GUI/WindowServer.h>

#include <stdlib.h>
#include <unistd.h>

#include <sstream>

namespace Lemon::GUI {
Window::Window(const char* title, vector2i_t size, uint32_t flags, int type, vector2i_t pos)
    : rootContainer({{0, 0}, size}), m_flags(flags), m_windowType(type) {
    rootContainer.SetWindow(this);

    WindowServer* server = WindowServer::Instance();
    rootContainer.background = Theme::Current().ColourBackground();

    auto response = server->CreateWindow(pos.x, pos.y, size.x, size.y, flags, title);

    m_windowBufferKey = response.bufferKey;
    m_windowID = response.windowID;

    server->RegisterWindow(this);

    if (m_windowBufferKey <= 0) {
        printf("[LibLemon] Warning: Window::Window: Failed to find window buffer!\n");
        return;
    }

    m_windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(m_windowBufferKey);

    m_windowBufferInfo->currentBuffer = 1;
    m_buffer1 = ((uint8_t*)m_windowBufferInfo) + m_windowBufferInfo->buffer1Offset;
    m_buffer2 = ((uint8_t*)m_windowBufferInfo) + m_windowBufferInfo->buffer2Offset;

    surface.buffer = m_buffer1;
    surface.width = size.x;
    surface.height = size.y;
}

Window::~Window() {
    WindowServer* server = WindowServer::Instance();

    server->DestroyWindow(m_windowID);
    server->UnregisterWindow(m_windowID);

    Lemon::UnmapSharedMemory(m_windowBufferInfo, m_windowBufferKey);
}

void Window::SetTitle(const std::string& title) { WindowServer::Instance()->SetTitle(m_windowID, title); }

void Window::Relocate(vector2i_t pos) { WindowServer::Instance()->Relocate(m_windowID, pos.x, pos.y); }

void Window::Resize(vector2i_t size) {
    Lemon::UnmapSharedMemory(m_windowBufferInfo, m_windowBufferKey);

    if (menuBar) {
        rootContainer.SetBounds({{0, WINDOW_MENUBAR_HEIGHT}, {size.x, size.y - WINDOW_MENUBAR_HEIGHT}});
    } else {
        rootContainer.SetBounds({{0, 0}, size});
    }

    rootContainer.UpdateFixedBounds();

    m_windowBufferKey = WindowServer::Instance()->Resize(m_windowID, size.x, size.y).bufferKey;
    if (m_windowBufferKey <= 0) {
        printf("[LibLemon] Warning: Window::Resize: Failed to obtain window buffer!\n");

        surface.buffer = nullptr;
        return;
    }

    m_windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(m_windowBufferKey);

    m_windowBufferInfo->currentBuffer = 0;
    m_buffer1 = ((uint8_t*)m_windowBufferInfo) + m_windowBufferInfo->buffer1Offset;
    m_buffer2 = ((uint8_t*)m_windowBufferInfo) + m_windowBufferInfo->buffer2Offset;

    surface.buffer = m_buffer1;
    surface.width = size.x;
    surface.height = size.y;

    Paint();
}

void Window::Minimize(int windowID, bool minimized) { WindowServer::Instance()->Minimize(windowID, minimized); }

void Window::UpdateFlags(uint32_t flags) {
    m_flags = flags;
    WindowServer::Instance()->UpdateFlags(m_windowID, flags);
}

void Window::SwapBuffers() {
    if (m_windowBufferInfo->drawing)
        return;

    if (surface.buffer == m_buffer1) {
        m_windowBufferInfo->currentBuffer = 0;
        surface.buffer = m_buffer2;
    } else {
        m_windowBufferInfo->currentBuffer = 1;
        surface.buffer = m_buffer1;
    }

    m_windowBufferInfo->dirty = 1;
}

void Window::Paint() {
    if (OnPaint)
        OnPaint(&surface);

    if (m_windowType == WindowType::GUI) {
        if (menuBar) {
            menuBar->Paint(&surface);
        }

        rootContainer.Paint(&surface);
    }

    if (OnPaintEnd)
        OnPaintEnd(&surface);

    SwapBuffers();
}

bool Window::PollEvent(LemonEvent& ev) {
    if (!m_eventQueue.empty()) {
        ev = m_eventQueue.front();
        m_eventQueue.pop();

        return true;
    }
    return false;
}

void Window::GUIPollEvents(){
    LemonEvent ev;
    while(PollEvent(ev)){
        // If the handler returns true, the event is not processed.
        auto handler = m_eventHandlers.find(ev.event);
        if(handler != m_eventHandlers.end() && handler->second(ev)){
            continue;
        }

        GUIHandleEvent(ev);
    }

    if(m_shouldResize){
        Resize(m_resizeBounds);
        m_shouldResize = false;
    }
}

void Window::GUIHandleEvent(LemonEvent& ev) {
    switch (ev.event) {
    case EventMousePressed: {
        lastMousePos = ev.mousePos;

        if (menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height) {
            rootContainer.active = nullptr;
            menuBar->OnMouseDown(ev.mousePos);
        } else if (menuBar) {
            // ev.mousePos.y -= menuBar->GetFixedBounds().height;
        }

        timespec newClick;
        clock_gettime(CLOCK_BOOTTIME, &newClick);

        if ((newClick.tv_nsec / 1000000 + newClick.tv_sec * 1000) -
                (m_lastClick.tv_nsec / 1000000 + m_lastClick.tv_sec * 1000) <
            600) { // Douuble click if clicks less than 600ms apart
            rootContainer.OnDoubleClick(ev.mousePos);

            newClick.tv_nsec = 0;
            newClick.tv_sec = 0; // Prevent a third click from being registerd as a double click
        } else {
            rootContainer.OnMouseDown(ev.mousePos);
        }

        m_lastClick = newClick;
    } break;
    case EventMouseReleased:
        lastMousePos = ev.mousePos;

        if (menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height) {
            rootContainer.active = nullptr;
            menuBar->OnMouseDown(ev.mousePos);
        } else if (menuBar) {
            // ev.mousePos.y -= menuBar->GetFixedBounds().height;
        }

        rootContainer.OnMouseUp(ev.mousePos);
        break;
    case EventRightMousePressed:
        lastMousePos = ev.mousePos;

        if (menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height) {
            rootContainer.active = nullptr;
            menuBar->OnMouseDown(ev.mousePos);
        } else if (menuBar) {
            // ev.mousePos.y -= menuBar->GetFixedBounds().height;
        }

        rootContainer.OnRightMouseDown(ev.mousePos);
        break;
    case EventRightMouseReleased:
        lastMousePos = ev.mousePos;

        if (menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height) {
            rootContainer.active = nullptr;
            menuBar->OnMouseDown(ev.mousePos);
        } else if (menuBar) {
            // ev.mousePos.y -= menuBar->GetFixedBounds().height;
        }

        rootContainer.OnRightMouseUp(ev.mousePos);
        break;
    case EventMouseExit:
        lastMousePos = {INT_MIN, INT_MIN}; // Prevent anything from staying selected
        rootContainer.OnMouseExit(ev.mousePos);

        break;
    case EventMouseEnter:
    case EventMouseMoved:
        lastMousePos = ev.mousePos;

        if (menuBar && ev.mousePos.y >= 0 && ev.mousePos.y < menuBar->GetFixedBounds().height) {
            menuBar->OnMouseMove(ev.mousePos);
        } else if (menuBar) {
            // ev.mousePos.y -= menuBar->GetFixedBounds().height;
        }

        rootContainer.OnMouseMove(ev.mousePos);
        break;
    case EventKeyPressed:
        rootContainer.OnKeyPress(ev.key);
        break;
    case EventKeyReleased:
        break;
    case EventWindowResize:
        if (m_flags & WINDOW_FLAGS_RESIZABLE) {
            m_shouldResize = true;
            m_resizeBounds = ev.resizeBounds;
        }
        break;
    case EventWindowMinimized:
        if (m_tooltipWindow.get()) {
            m_tooltipWindow->Minimize(true); // Make sure the tooltip is also minimized
        }
        break;
    case EventWindowClosed:
        closed = true;
        break;
    case EventWindowCommand:
        if (menuBar && !rootContainer.active && OnMenuCmd) {
            OnMenuCmd(ev.windowCmd, this);
        } else {
            rootContainer.OnCommand(ev.windowCmd);
        }
        break;
    }
}

void Window::AddWidget(Widget* w) { rootContainer.AddWidget(w); }

void Window::RemoveWidget(Widget* w) { rootContainer.RemoveWidget(w); }

void Window::SetActive(Widget* w) { rootContainer.active = w; }

void Window::DisplayContextMenu(const std::vector<ContextMenuEntry>& entries, vector2i_t pos) {
    if (pos.x == -1 && pos.y == -1) {
        pos = lastMousePos;
    }

    std::ostringstream serializedEntries;
    for (const ContextMenuEntry& ent : entries) {
        serializedEntries << ent.id << "," << ent.name << ";";
    }

    std::string str = serializedEntries.str();

    WindowServer::Instance()->DisplayContextMenu(m_windowID, pos.x, pos.y, str);
}

void Window::CreateMenuBar() {
    menuBar = new WindowMenuBar();
    menuBar->SetWindow(this);

    rootContainer.SetBounds({0, WINDOW_MENUBAR_HEIGHT, surface.width, surface.height - WINDOW_MENUBAR_HEIGHT});
}

void Window::SetTooltip(const char* text, vector2i_t pos) {
    if (!m_tooltipWindow.get()) {
        m_tooltipWindow = std::make_unique<TooltipWindow>(text, pos, Theme::Current().ColourContentBackground());
    } else {
        m_tooltipWindow->SetText(text);

        m_tooltipWindow->Minimize(false);
        m_tooltipWindow->Relocate(pos);
    }
}

Window::TooltipWindow::TooltipWindow(const char* text, vector2i_t pos, const RGBAColour& bgColour)
    : LemonWMServerEndpoint("lemon.lemonwm/Instance"), textObject({4, 4}, text), backgroundColour(bgColour) {
    auto response = CreateWindow(pos.x, pos.y, textObject.Size().x + 8, textObject.Size().y + 8,
                                 WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL | WINDOW_FLAGS_TOOLTIP, "");

    windowID = response.windowID;
    windowBufferKey = response.bufferKey;

    if (windowBufferKey <= 0) {
        printf("[LibLemon] Warning: Window::TooltipWindow::TooltipWindow: Failed to obtain window buffer!\n");
        return;
    }

    textObject.SetColour(Theme::Current().ColourText());

    windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);

    windowBufferInfo->currentBuffer = 0;
    buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
    buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

    surface.buffer = buffer1;
    surface.width = textObject.Size().x + 8;
    surface.height = textObject.Size().y + 8;

    Paint();
}

void Window::TooltipWindow::SetText(const char* text) {
    textObject.SetText(text);

    if (Lemon::UnmapSharedMemory(windowBufferInfo, windowBufferKey)) {
        throw std::runtime_error("Failed to unmap shared memory!");
        return;
    }
    surface.buffer = nullptr; // Invalidate surface buffer

    windowBufferKey = Resize(windowID, textObject.Size().x + 8, textObject.Size().y + 8).bufferKey;
    if (windowBufferKey <= 0) {
        printf("[LibLemon] Warning: Window::TooltipWindow::Resize: Failed to obtain window buffer!\n");
        return;
    }

    windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);

    windowBufferInfo->currentBuffer = 1;
    buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
    buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

    surface.buffer = buffer1;
    surface.width = textObject.Size().x + 8;
    surface.height = textObject.Size().y + 8;

    Paint();
}

void Window::TooltipWindow::Paint() {
    if (!surface.buffer) {
        return;
    }

    Graphics::DrawRect(0, 0, surface.width, surface.height, backgroundColour, &surface);

    textObject.BlitTo(&surface);

    if (surface.buffer == buffer1) {
        windowBufferInfo->currentBuffer = 0;
        surface.buffer = buffer2;
    } else {
        windowBufferInfo->currentBuffer = 1;
        surface.buffer = buffer1;
    }

    windowBufferInfo->dirty = 1;
}

void WindowMenuBar::Paint(surface_t* surface) {
    fixedBounds = {0, 0, window->GetSize().x, WINDOW_MENUBAR_HEIGHT};

    Graphics::DrawRect(fixedBounds, Theme::Current().ColourBackground(), surface);
    Graphics::DrawRect(0, fixedBounds.height, fixedBounds.width, 1, Theme::Current().ColourForeground(), surface);

    int xpos = 0;
    for (auto& item : items) {
        xpos += Graphics::DrawString(item.first.c_str(), xpos + 4, 4, Theme::Current().ColourText(), surface) + 8;
    }
}

void WindowMenuBar::OnMouseDown(vector2i_t mousePos) {
    fixedBounds = {0, 0, window->GetSize().x, WINDOW_MENUBAR_HEIGHT};

    int xpos = 0;
    for (auto& item : items) {
        int oldpos = xpos;
        xpos += Graphics::GetTextLength(item.first.c_str()) + 8; // 4 pixels on each side of padding
        if (xpos >= mousePos.x) {
            window->DisplayContextMenu(item.second, {oldpos, WINDOW_MENUBAR_HEIGHT});
            break;
        }
    }
}

void WindowMenuBar::OnMouseUp(__attribute__((unused)) vector2i_t mousePos) {}

void WindowMenuBar::OnMouseMove(__attribute__((unused)) vector2i_t mousePos) {}
} // namespace Lemon::GUI
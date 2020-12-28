#include <lemon/gui/window.h>
#include <lemon/core/sharedmem.h>

#include <stdlib.h>
#include <unistd.h>

#include <sstream>

namespace Lemon::GUI{
    Window::Window(const char* title, vector2i_t size, uint32_t flags, int type, vector2i_t pos) : rootContainer({{0, 0}, size}) {
        windowType = type;
        this->flags = flags;

        rootContainer.window = this;

        windowBufferKey = CreateWindow(pos.x, pos.y, size.x, size.y, flags, title);
        if(windowBufferKey <= 0){
            printf("[LibLemon] Warning: Window::Window: Failed to obtain window buffer!\n");
            return;
        }

        windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);

        windowBufferInfo->currentBuffer = 0;
        buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
        buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

        surface.buffer = buffer1;
        surface.width = size.x;
        surface.height = size.y;
    }

    Window::~Window(){
        DestroyWindow();

        Lemon::UnmapSharedMemory(windowBufferInfo, windowBufferKey);

        usleep(100);
    }

    void Window::Resize(vector2i_t size){
        
        Lemon::UnmapSharedMemory(windowBufferInfo, windowBufferKey);

        windowBufferKey = WMClient::Resize(size.x, size.y);
        if(windowBufferKey <= 0){
            printf("[LibLemon] Warning: Window::Resize: Failed to obtain window buffer!\n");
            return;
        }

        windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);

        windowBufferInfo->currentBuffer = 0;
        buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
        buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

        surface.buffer = buffer1;
        surface.width = size.x;
        surface.height = size.y;

        if(menuBar){
            rootContainer.SetBounds({{0, 16}, {size.x, size.y - WINDOW_MENUBAR_HEIGHT}});
        } else {
            rootContainer.SetBounds({{0, 0}, size});
        }

        rootContainer.UpdateFixedBounds();
    }

    void Window::SwapBuffers(){
        if(windowBufferInfo->drawing) return;

        if(surface.buffer == buffer1){
            windowBufferInfo->currentBuffer = 0;
            surface.buffer = buffer2;
        } else {
            windowBufferInfo->currentBuffer = 1;
            surface.buffer = buffer1;
        }

        windowBufferInfo->dirty = 1;
    }

    void Window::Paint(){
        if(OnPaint) OnPaint(&surface);
        
        if(windowType == WindowType::GUI) {
            if(menuBar){
                menuBar->Paint(&surface);
            }

            rootContainer.Paint(&surface);
        }

        SwapBuffers();
    }
    
    bool Window::PollEvent(LemonEvent& ev){
        Message m;
        if(long ret = Poll(m); ret > 0 && m.id() == WindowEvent){
            if(!m.Decode(ev)) {
                return true;
            }
        }

        return false;
    }
    
    void Window::WaitEvent(){
        
    }

    void Window::GUIHandleEvent(LemonEvent& ev){
        switch(ev.event){
            case EventMousePressed:
                {
                    lastMousePos = ev.mousePos;

                    if(menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height){
                        rootContainer.active = nullptr;
                        menuBar->OnMouseDown(ev.mousePos);
                    } else if (menuBar){
                        //ev.mousePos.y -= menuBar->GetFixedBounds().height;
                    }

                    timespec newClick;
                    clock_gettime(CLOCK_BOOTTIME, &newClick);

                    if((newClick.tv_nsec / 1000000 + newClick.tv_sec * 1000) - (lastClick.tv_nsec / 1000000 + lastClick.tv_sec * 1000) < 600){ // Douuble click if clicks less than 600ms apart
                        rootContainer.OnDoubleClick(ev.mousePos);
                        
                        newClick.tv_nsec = 0;
                        newClick.tv_sec = 0; // Prevent a third click from being registerd as a double click
                    } else {
                        rootContainer.OnMouseDown(ev.mousePos);
                    }

                    lastClick = newClick;
                }
                break;
            case EventMouseReleased:
                lastMousePos = ev.mousePos;
                
                if(menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height){
                    rootContainer.active = nullptr;
                    menuBar->OnMouseDown(ev.mousePos);
                } else if (menuBar){
                    //ev.mousePos.y -= menuBar->GetFixedBounds().height;
                }

                rootContainer.OnMouseUp(ev.mousePos);
                break;
            case EventRightMousePressed:
                lastMousePos = ev.mousePos;

                if(menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height){
                    rootContainer.active = nullptr;
                    menuBar->OnMouseDown(ev.mousePos);
                } else if (menuBar){
                    //ev.mousePos.y -= menuBar->GetFixedBounds().height;
                }

                rootContainer.OnRightMouseDown(ev.mousePos);
                break;
            case EventRightMouseReleased:
                lastMousePos = ev.mousePos;

                if(menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height){
                    rootContainer.active = nullptr;
                    menuBar->OnMouseDown(ev.mousePos);
                } else if (menuBar){
                    //ev.mousePos.y -= menuBar->GetFixedBounds().height;
                }

                rootContainer.OnRightMouseUp(ev.mousePos);
                break;
            case EventMouseMoved:
                lastMousePos = ev.mousePos;

                if(menuBar && ev.mousePos.y < menuBar->GetFixedBounds().height){
                    menuBar->OnMouseMove(ev.mousePos);
                } else if (menuBar){
                    //ev.mousePos.y -= menuBar->GetFixedBounds().height;
                }

                rootContainer.OnMouseMove(ev.mousePos);
                break;
            case EventKeyPressed:
                rootContainer.OnKeyPress(ev.key);
                break;
            case EventKeyReleased:
                break;
            case EventWindowResize:
                if(flags & WINDOW_FLAGS_RESIZABLE){
                    Resize(ev.resizeBounds);
                }
                break;
            case EventWindowClosed:
                closed = true;
                break;
            case EventWindowCommand:
                if(menuBar && !rootContainer.active && OnMenuCmd){
                    OnMenuCmd(ev.windowCmd, this);
                } else {
                    rootContainer.OnCommand(ev.windowCmd);
                }
                break;
        }
    }

    void Window::AddWidget(Widget* w){
        rootContainer.AddWidget(w);
    }

    void Window::RemoveWidget(Widget* w){
        rootContainer.RemoveWidget(w);
    }

    void Window::DisplayContextMenu(const std::vector<ContextMenuEntry>& entries, vector2i_t pos){
        if(pos.x == -1 && pos.y == -1){
            pos = lastMousePos;
        }

        std::ostringstream serializedEntries;
        for(const ContextMenuEntry& ent : entries){
            serializedEntries << ent.id << "," << ent.name << ";";
        }

        std::string str = serializedEntries.str();
        
        WMClient::DisplayContextMenu(pos.x, pos.y, str);
    }

    void Window::CreateMenuBar(){
        menuBar = new WindowMenuBar();
        menuBar->window = this;

        rootContainer.SetBounds({0, WINDOW_MENUBAR_HEIGHT, surface.width, surface.height - WINDOW_MENUBAR_HEIGHT});
    }

    void WindowMenuBar::Paint(surface_t* surface){
        fixedBounds = {0, 0, window->GetSize().x, WINDOW_MENUBAR_HEIGHT};

        Graphics::DrawRect(fixedBounds, colours[Colour::Background], surface);
        Graphics::DrawRect(0, fixedBounds.height, fixedBounds.width, 1, colours[Colour::Foreground], surface);

        int xpos = 0;
        for(auto& item : items){
            xpos += Graphics::DrawString(item.first.c_str(), xpos + 4, 4, colours[Colour::Text], surface) + 8;
        }
    }

    void WindowMenuBar::OnMouseDown(vector2i_t mousePos){
        fixedBounds = {0, 0, window->GetSize().x, WINDOW_MENUBAR_HEIGHT};

        int xpos = 0;
        for(auto& item : items){
            int oldpos = xpos;
            xpos += Graphics::GetTextLength(item.first.c_str()) + 8; // 4 pixels on each side of padding
            if(xpos >= mousePos.x){
                window->DisplayContextMenu(item.second, {oldpos, WINDOW_MENUBAR_HEIGHT});
                break;
            }
        }
    }

    void WindowMenuBar::OnMouseUp(__attribute__((unused)) vector2i_t mousePos){
        
    }

    void WindowMenuBar::OnMouseMove(__attribute__((unused)) vector2i_t mousePos){
        
    }

    int64_t WMClient::CreateWindow(int x, int y, int width, int height, unsigned int flags, const std::string& title) const {
        Message ret;
        
        Call(Message(WMCreateWindow, x, y, width, height, flags, title), ret, WindowBufferReturn);

        int64_t key = 0;
        if(ret.Decode(key)){
            return 0;
        }

        return key;
    }

    void WMClient::DestroyWindow() const {
        Queue(Message(WMDestroyWindow));
    }

    void WMClient::SetTitle(const std::string& title) const {
        Queue(Message(WMSetWindowTitle, title));
    }

    void WMClient::Relocate(int x, int y) const {
        Queue(Message(WMCreateWindow, x, y));
    }

    uint64_t WMClient::Resize(int width, int height) const {
        Queue(Message(WMResizeWindow, width, height));

        return -1;
    }

    void WMClient::Minimize(bool minimized) const {
        Queue(Message(WMMinimizeWindow, minimized));
    }

    void WMClient::Minimize(long windowID, bool minimized) const {
        Queue(Message(WMMinimizeOtherWindow, windowID, minimized));
    }

    void WMClient::DisplayContextMenu(int x, int y, const std::string& serializedEntries) const {
        Queue(Message(WMDisplayContextMenu, x, y, serializedEntries));
    }

    void WMClient::InitializeShellConnection() const {
        Queue(WMInitializeShellConnection, nullptr, 0);
    }
}
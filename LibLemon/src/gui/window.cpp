#include <gui/window.h>
#include <core/sharedmem.h>

#include <stdlib.h>

#include <unistd.h>

namespace Lemon::GUI{
    Window::Window(const char* title, vector2i_t size, uint32_t flags, int type, vector2i_t pos) : rootContainer({{0, 0}, size}) {
        windowType = type;
        this->flags = flags;
        
        sockaddr_un sockAddr;
        strcpy(sockAddr.sun_path, wmSocketAddress);
        sockAddr.sun_family = AF_UNIX;

        msgClient.Connect(sockAddr, sizeof(sockaddr_un)); // Connect to Window Manager

        LemonMessage* createMsg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand) + strlen(title));
    
        size_t windowBufferSize = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F)) + ((size.x * size.y * 4 + 0x1F) & (~0x1F) /* Round up to 32 bytes*/) * 2;

        windowBufferKey = Lemon::CreateSharedMemory(windowBufferSize, SMEM_FLAGS_SHARED);
        windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);
        windowBufferInfo->currentBuffer = 0;
        windowBufferInfo->buffer1Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F));
        windowBufferInfo->buffer2Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F)) + ((size.x * size.y * 4 + 0x1F) & (~0x1F) /* Round up to 32 bytes*/);

        buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
        buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

        surface.buffer = buffer1;
        surface.width = size.x;
        surface.height = size.y;

        WMCommand* cmd = (WMCommand*)createMsg->data;
        cmd->cmd = WMCreateWindow;
        cmd->create.pos = pos;
        cmd->create.size = size;
        cmd->create.flags = flags;
        cmd->create.bufferKey = windowBufferKey;
        memcpy(cmd->create.title, title, strlen(title));
        cmd->create.titleLength = strlen(title);

        createMsg->length = sizeof(WMCommand) + strlen(title);
        createMsg->protocol = LEMON_MESSAGE_PROTOCOL_WMCMD;
        
        msgClient.Send(createMsg);

        free(createMsg);

        rootContainer.window = this;
    }

    Window::~Window(){
        LemonMessage* destroyMsg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand));

        WMCommand* cmd = (WMCommand*)destroyMsg->data;
        cmd->cmd = WMDestroyWindow;

        destroyMsg->length = sizeof(WMCommand);
        destroyMsg->protocol = LEMON_MESSAGE_PROTOCOL_WMCMD;

        msgClient.Send(destroyMsg);

        free(destroyMsg);

        usleep(100);
    }

    void Window::SetTitle(const char* title){
        msgClient.Send(Lemon::Message(LEMON_MESSAGE_PROTOCOL_WMCMD, static_cast<unsigned short>(WMSetTitle), Lemon::Message::EncodeString(title)));
    }

    void Window::Minimize(bool minimized){
        LemonMessage* msg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand));

        WMCommand* cmd = (WMCommand*)msg->data;
        cmd->cmd = WMMinimize;
        cmd->minimized = minimized;

        msg->length = sizeof(WMCommand);
        msg->protocol = LEMON_MESSAGE_PROTOCOL_WMCMD;

        msgClient.Send(msg);

        free(msg);
    }
    
    void Window::Minimize(int windowID, bool minimized){
        LemonMessage* msg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand));

        WMCommand* cmd = (WMCommand*)msg->data;
        cmd->cmd = WMMinimizeOther;
        cmd->minimized = minimized;
        cmd->minimizeWindowID = windowID;

        msg->length = sizeof(WMCommand);
        msg->protocol = LEMON_MESSAGE_PROTOCOL_WMCMD;

        msgClient.Send(msg);

        free(msg);
    }

    void Window::Resize(vector2i_t size){
        Lemon::UnmapSharedMemory(windowBufferInfo, windowBufferKey);

        size_t windowBufferSize = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F)) + ((size.x * size.y * 4 + 0x1F) & (~0x1F) /* Round up to 32 bytes*/) * 2;

        windowBufferKey = Lemon::CreateSharedMemory(windowBufferSize, SMEM_FLAGS_SHARED);
        windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);
        windowBufferInfo->currentBuffer = 0;
        windowBufferInfo->buffer1Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F));
        windowBufferInfo->buffer2Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F)) + ((size.x * size.y * 4 + 0x1F) & (~0x1F) /* Round up to 32 bytes*/);

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

        LemonMessage* msg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand));

        WMCommand* cmd = (WMCommand*)msg->data;
        cmd->cmd = WMResize;

        cmd->size = size;
        cmd->bufferKey = windowBufferKey;

        msg->length = sizeof(WMCommand);
        msg->protocol = LEMON_MESSAGE_PROTOCOL_WMCMD;

        msgClient.Send(msg);

        free(msg);

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
        if(windowType == WindowType::GUI) {
            if(menuBar){
                menuBar->Paint(&surface);
            }

            rootContainer.Paint(&surface);
        }

        if(OnPaint) OnPaint(&surface);

        SwapBuffers();
    }
    
    bool Window::PollEvent(LemonEvent& ev){
        if(auto m = msgClient.Poll()){
            if(m->protocol == LEMON_MESSAGE_PROTOCOL_WMEVENT){
                ev = *((LemonEvent*)m->data);
                return true;
            }
        }

        return false;
    }
    
    void Window::WaitEvent(){
        msgClient.Wait();
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

    void Window::DisplayContextMenu(std::vector<ContextMenuEntry>& entries, vector2i_t pos){
        if(pos.x == -1 && pos.y == -1){
            pos = lastMousePos;
        }

        unsigned cmdSize = sizeof(WMCommand);
        for(ContextMenuEntry& ent : entries){
            cmdSize += sizeof(WMContextMenuEntry) + ent.name.length();
        }

        LemonMessage* msg = (LemonMessage*)malloc(sizeof(LemonMessage) + cmdSize);
        msg->length = cmdSize;
        msg->protocol = LEMON_MESSAGE_PROTOCOL_WMCMD;

        WMCommand* cmd = (WMCommand*)msg->data;
        cmd->cmd = WMOpenContextMenu;
        cmd->contextMenu.contextEntryCount = entries.size();
        cmd->contextMenuPosition = pos;
        
        WMContextMenuEntry* wment = cmd->contextMenu.contextEntries;
        for(ContextMenuEntry& ent : entries){
            strncpy(wment->data, ent.name.c_str(), ent.name.length());
            wment->length = ent.name.length();
            wment->id = ent.id;
            wment = (WMContextMenuEntry*)(((void*)wment) + sizeof(WMContextMenuEntry) + ent.name.length());
        }

        msgClient.Send(msg);

        free(msg);
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
            xpos += Graphics::DrawString(item.first.c_str(), xpos + 4, 4, colours[Colour::TextDark], surface) + 8;
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
}
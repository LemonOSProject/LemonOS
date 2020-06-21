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
        cmd->length = sizeof(WMCreateWindowCommand) + strlen(title);
        cmd->cmd = WMCreateWindow;
        cmd->create.pos = pos;
        cmd->create.size = size;
        cmd->create.flags = flags;
        cmd->create.bufferKey = windowBufferKey;
        strncpy(cmd->create.title, title, strlen(title));
        cmd->create.titleLength = strlen(title);

        createMsg->length = sizeof(WMCommand) + strlen(title);
        createMsg->protocol = LEMON_MESSAGE_PROTCOL_WMCMD;
        
        msgClient.Send(createMsg);

        free(createMsg);

        rootContainer.window = this;
    }

    Window::~Window(){
        LemonMessage* destroyMsg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand));

        WMCommand* cmd = (WMCommand*)destroyMsg->data;
        cmd->cmd = WMDestroyWindow;

        destroyMsg->length = sizeof(WMCommand);
        destroyMsg->protocol = LEMON_MESSAGE_PROTCOL_WMCMD;

        msgClient.Send(destroyMsg);

        free(destroyMsg);

        usleep(100);
    }

    void Window::Minimize(bool minimized){
        LemonMessage* msg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand));

        WMCommand* cmd = (WMCommand*)msg->data;
        cmd->cmd = WMMinimize;
        cmd->minimized = minimized;

        msg->length = sizeof(WMCommand);
        msg->protocol = LEMON_MESSAGE_PROTCOL_WMCMD;

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
        msg->protocol = LEMON_MESSAGE_PROTCOL_WMCMD;

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
        rootContainer.SetBounds({{0, 0}, size});

        LemonMessage* msg = (LemonMessage*)malloc(sizeof(LemonMessage) + sizeof(WMCommand));

        WMCommand* cmd = (WMCommand*)msg->data;
        cmd->cmd = WMResize;

        cmd->size = size;
        cmd->bufferKey = windowBufferKey;

        msg->length = sizeof(WMCommand);
        msg->protocol = LEMON_MESSAGE_PROTCOL_WMCMD;

        msgClient.Send(msg);

        free(msg);
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
    }

    void Window::Paint(){

        if(OnPaint) OnPaint(&surface);
        
        if(windowType == WindowType::GUI) rootContainer.Paint(&surface);

        SwapBuffers();
    }
    
    bool Window::PollEvent(LemonEvent& ev){
        if(auto m = msgClient.Poll()){
            if(m->protocol == LEMON_MESSAGE_PROTCOL_WMEVENT){
                ev = *((LemonEvent*)m->data);
                return true;
            }
        }

        return false;
    }

    void Window::GUIHandleEvent(LemonEvent& ev){
        switch(ev.event){
            case EventMousePressed:
                {
                    timespec newClick;
                    clock_gettime(CLOCK_BOOTTIME, &newClick);

                    if((newClick.tv_nsec / 1000000 + newClick.tv_sec * 1000) - (lastClick.tv_nsec / 1000000 + lastClick.tv_sec * 1000) < 600){ // Douuble click if clicks less than 600ms apart
                        rootContainer.OnDoubleClick(ev.mousePos);
                    } else {
                        rootContainer.OnMouseDown(ev.mousePos);
                    }

                    lastClick = newClick;
                }
                break;
            case EventMouseReleased:
                rootContainer.OnMouseUp(ev.mousePos);
                break;
            case EventMouseMoved:
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
        }
    }

    void Window::AddWidget(Widget* w){
        rootContainer.AddWidget(w);
    }

    void Window::RemoveWidget(Widget* w){
        rootContainer.RemoveWidget(w);
    }
}
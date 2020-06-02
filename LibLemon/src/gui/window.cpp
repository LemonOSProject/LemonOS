#include <gui/window.h>
#include <core/sharedmem.h>

#include <stdlib.h>

namespace Lemon::GUI{
    Window::Window(const char* title, vector2i_t size, vector2i_t pos, uint32_t flags){
        sockaddr_un sockAddr;
        strcpy(sockAddr.sun_path, wmSocketAddress);
        sockAddr.sun_family = AF_UNIX;

        msgClient.Connect(sockAddr, sizeof(sockaddr_un));

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
    }

    void Window::SwapBuffers(){
        if(surface.buffer == buffer1){
            windowBufferInfo->currentBuffer = 0;
            surface.buffer = buffer2;
        } else {
            windowBufferInfo->currentBuffer = 1;
            surface.buffer = buffer1;
        }
    }

    void Window::Paint(){
        OnPaint(&surface);

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
}
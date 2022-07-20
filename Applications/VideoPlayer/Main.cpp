#include <Lemon/GUI/Window.h>

#include <Lemon/Graphics/Graphics.h>

#include "StreamContext.h"

using namespace Lemon;
using namespace Lemon::GUI;

Window* window;
void FlipBuffers() {
    window->SwapBuffers();
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }

    StreamContext* ctx = new StreamContext();

    window = new Window("Video Player", {1280, 720}, WINDOW_FLAGS_RESIZABLE, WindowType::Basic);
    
    ctx->surfaceLock.lock();
    ctx->SetDisplaySurface(&window->surface, window->GetRect());
    ctx->surfaceLock.unlock();
    
    ctx->FlipBuffers = FlipBuffers;

    if(int e = ctx->PlayTrack(argv[1]); e) {
        return e;
    }

    window->SetTitle(argv[1]);

    while (!window->closed) {
        Lemon::WindowServer::Instance()->Poll();

        LemonEvent ev;
        while (window->PollEvent(ev)) {
            if(ev.event == EventWindowClosed) {
                window->closed = true;
                break;
            } else if(ev.event == EventMousePressed) {
                if(ctx->IsAudioPlaying()) {
                    ctx->PlaybackPause();
                } else {
                    ctx->PlaybackStart();
                }
            } else if(ev.event == EventWindowResize) {
                ctx->surfaceLock.lock();
                window->Resize(ev.resizeBounds);
                ctx->SetDisplaySurface(&window->surface, window->GetRect());
                ctx->surfaceLock.unlock();
            }
        }
    }

    delete window;
    delete ctx;
    return 0;
}

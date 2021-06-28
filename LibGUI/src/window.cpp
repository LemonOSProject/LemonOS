#include <Lemon/GUI/Window.h>
#include <Lemon/Core/SharedMemory.h>
#include <Lemon/Core/JSON.h>

#include <stdlib.h>
#include <unistd.h>

#include <sstream>

namespace Lemon::GUI{
    Window::Window(const char* title, vector2i_t size, uint32_t flags, int type, vector2i_t pos)
        : LemonWMServerEndpoint("lemon.lemonwm/Instance"), rootContainer({{0, 0}, size}) {
        windowType = type;
        this->flags = flags;

        rootContainer.SetWindow(this);

        auto response = CreateWindow(pos.x, pos.y, size.x, size.y, flags, title);

        windowBufferKey = response.bufferKey;
        windowID = response.windowID;

        if(windowBufferKey <= 0){
            printf("[LibLemon] Warning: Window::Window: Failed to obtain window buffer!\n");
            return;
        }

        windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);

        windowBufferInfo->currentBuffer = 1;
        buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
        buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

        surface.buffer = buffer1;
        surface.width = size.x;
        surface.height = size.y;
    }

    Window::~Window(){
        DestroyWindow();

        Lemon::UnmapSharedMemory(windowBufferInfo, windowBufferKey);
    }

    void Window::UpdateGUITheme(const std::string& path){
        JSONParser jP = JSONParser(path.c_str());
        auto json = jP.Parse();

        if(json.IsObject()){
            JSONValue obj;
            if(json.object->find("gui") != json.object->end() && (obj = json.object->at("gui")).IsObject()) {
                std::map<Lemon::JSONKey, Lemon::JSONValue>& values = *obj.object;

                if(auto it = values.find("background"); it != values.end()){
                    auto& v = it->second;

                    if(v.IsArray() && v.array->size() >= 3){
                        Lemon::colours[Lemon::Colour::Background] = (RGBAColour){v.array->at(0).AsUnsignedNumber<uint8_t>(), v.array->at(1).AsUnsignedNumber<uint8_t>(), v.array->at(2).AsUnsignedNumber<uint8_t>(), 255};
                    } else {} // TODO: Do something when invalid valaue
                }

                if(auto it = values.find("content"); it != values.end()){
                    auto& v = it->second;

                    if(v.IsArray() && v.array->size() >= 3){
                        Lemon::colours[Lemon::Colour::ContentBackground] = (RGBAColour){v.array->at(0).AsUnsignedNumber<uint8_t>(), v.array->at(1).AsUnsignedNumber<uint8_t>(), v.array->at(2).AsUnsignedNumber<uint8_t>(), 255};
                    } else {} // TODO: Do something when invalid valaue
                }

                if(auto it = values.find("text"); it != values.end()){
                    auto& v = it->second;

                    if(v.IsArray() && v.array->size() >= 3){
                        Lemon::colours[Lemon::Colour::Text] = (RGBAColour){v.array->at(0).AsUnsignedNumber<uint8_t>(), v.array->at(1).AsUnsignedNumber<uint8_t>(), v.array->at(2).AsUnsignedNumber<uint8_t>(), 255};
                    } else {} // TODO: Do something when invalid valaue
                }

                if(auto it = values.find("textAlternate"); it != values.end()){
                    auto& v = it->second;

                    if(v.IsArray() && v.array->size() >= 3){
                        Lemon::colours[Lemon::Colour::TextAlternate] = (RGBAColour){v.array->at(0).AsUnsignedNumber<uint8_t>(), v.array->at(1).AsUnsignedNumber<uint8_t>(), v.array->at(2).AsUnsignedNumber<uint8_t>(), 255};
                    } else {} // TODO: Do something when invalid valaue
                }

                if(auto it = values.find("highlight"); it != values.end()){
                    auto& v = it->second;

                    if(v.IsArray() && v.array->size() >= 3){
                        Lemon::colours[Lemon::Colour::Foreground] = (RGBAColour){v.array->at(0).AsUnsignedNumber<uint8_t>(), v.array->at(1).AsUnsignedNumber<uint8_t>(), v.array->at(2).AsUnsignedNumber<uint8_t>(), 255};
                    } else {} // TODO: Do something when invalid valaue
                }

                if(auto it = values.find("contentShadow"); it != values.end()){
                    auto& v = it->second;

                    if(v.IsArray() && v.array->size() >= 3){
                        Lemon::colours[Lemon::Colour::ContentShadow] = (RGBAColour){v.array->at(0).AsUnsignedNumber<uint8_t>(), v.array->at(1).AsUnsignedNumber<uint8_t>(), v.array->at(2).AsUnsignedNumber<uint8_t>(), 255};
                    } else {} // TODO: Do something when invalid valaue
                }
            }
        } else {
            printf("[LibLemon] Warning: Failed to parse system theme!\n");
        }
    }

    void Window::Resize(vector2i_t size){
        Lemon::UnmapSharedMemory(windowBufferInfo, windowBufferKey);

        if(menuBar){
            rootContainer.SetBounds({{0, 16}, {size.x, size.y - WINDOW_MENUBAR_HEIGHT}});
        } else {
            rootContainer.SetBounds({{0, 0}, size});
        }

        rootContainer.UpdateFixedBounds();

        windowBufferKey = LemonWMServerEndpoint::Resize(size.x, size.y).bufferKey;
        if(windowBufferKey <= 0){
            printf("[LibLemon] Warning: Window::Resize: Failed to obtain window buffer!\n");

            surface.buffer = nullptr;
            return;
        }

        windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);

        windowBufferInfo->currentBuffer = 0;
        buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
        buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

        surface.buffer = buffer1;
        surface.width = size.x;
        surface.height = size.y;

        Paint();
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

        if(OnPaintEnd) OnPaintEnd(&surface);

        SwapBuffers();
    }
    
    bool Window::PollEvent(LemonEvent& ev){
        Message m;
        long ret;
        while((ret = Poll(m)) > 0){
            HandleMessage(handle, m);
        }

        if(!eventQueue.empty()){
            ev = eventQueue.front();
            eventQueue.pop();

            return true;
        }
        return false;
    }
    
    void Window::WaitEvent(long tm){
        Wait(tm);
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
            case EventMouseExit:
                lastMousePos = {INT_MIN, INT_MIN}; // Prevent anything from staying selected
                rootContainer.OnMouseExit(ev.mousePos);

                break;
            case EventMouseEnter:
            case EventMouseMoved:
                lastMousePos = ev.mousePos;
                
                if(menuBar && ev.mousePos.y >= 0 && ev.mousePos.y < menuBar->GetFixedBounds().height){
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
            case EventWindowMinimized:
                if(tooltipWindow.get()){
                    tooltipWindow->Minimize(true); // Make sure the tooltip is also minimized
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

    void Window::SetActive(Widget* w){
        rootContainer.active = w;
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
        
        LemonWMServerEndpoint::DisplayContextMenu(pos.x, pos.y, str);
    }

    void Window::OnPeerDisconnect(handle_t){
        throw std::runtime_error("Lost connected to window server!");
    }

    void Window::OnSendEvent(handle_t, int32_t id, uint64_t data){
        eventQueue.push(LemonEvent{ .event = id, .data = data });
    }

    void Window::OnPing(handle_t){
        Pong();
    }

    void Window::CreateMenuBar(){
        menuBar = new WindowMenuBar();
        menuBar->SetWindow(this);

        rootContainer.SetBounds({0, WINDOW_MENUBAR_HEIGHT, surface.width, surface.height - WINDOW_MENUBAR_HEIGHT});
    }

    void Window::SetTooltip(const char* text, vector2i_t pos){
        if(!tooltipWindow.get()){
            tooltipWindow = std::make_unique<TooltipWindow>(text, pos, colours[Colour::ContentBackground]);
        } else {
            tooltipWindow->SetText(text);

            tooltipWindow->Minimize(false);
            tooltipWindow->Relocate(pos);
        }
    }

    Window::TooltipWindow::TooltipWindow(const char* text, vector2i_t pos, const RGBAColour& bgColour)
         : LemonWMServerEndpoint("lemon.lemonwm/Instance"), textObject({4, 4}, text), backgroundColour(bgColour) {
        auto response = CreateWindow(pos.x, pos.y, textObject.Size().x + 8, textObject.Size().y + 8, WINDOW_FLAGS_NODECORATION | WINDOW_FLAGS_NOSHELL | WINDOW_FLAGS_TOOLTIP, "");
        
        windowID = response.windowID;
        windowBufferKey = response.bufferKey;
        
        if(windowBufferKey <= 0){
            printf("[LibLemon] Warning: Window::TooltipWindow::TooltipWindow: Failed to obtain window buffer!\n");
            return;
        }

        textObject.SetColour(colours[Colour::Text]);

        windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);

        windowBufferInfo->currentBuffer = 0;
        buffer1 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer1Offset;
        buffer2 = ((uint8_t*)windowBufferInfo) + windowBufferInfo->buffer2Offset;

        surface.buffer = buffer1;
        surface.width = textObject.Size().x + 8;
        surface.height = textObject.Size().y + 8;

        Paint();
    }

    void Window::TooltipWindow::SetText(const char* text){
        textObject.SetText(text);

        if(Lemon::UnmapSharedMemory(windowBufferInfo, windowBufferKey)){
            throw std::runtime_error("Failed to unmap shared memory!");
            return;
        }
        surface.buffer = nullptr; // Invalidate surface buffer

        windowBufferKey = Resize(textObject.Size().x + 8, textObject.Size().y + 8).bufferKey;
        if(windowBufferKey <= 0){
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

    void Window::TooltipWindow::Paint(){
        if(!surface.buffer){
            return;
        }

        Graphics::DrawRect(0, 0, surface.width, surface.height, backgroundColour, &surface);

        textObject.Render(&surface);

        if(surface.buffer == buffer1){
            windowBufferInfo->currentBuffer = 0;
            surface.buffer = buffer2;
        } else {
            windowBufferInfo->currentBuffer = 1;
            surface.buffer = buffer1;
        }

        windowBufferInfo->dirty = 1;
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
}
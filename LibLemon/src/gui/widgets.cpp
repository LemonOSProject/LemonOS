#include <gui/widgets.h>

#include <core/keyboard.h>
#include <ctype.h>
#include <string>
#include <math.h>

namespace Lemon::GUI {
    Widget::Widget() {}

    Widget::Widget(rect_t bounds, LayoutSize newSizeX, LayoutSize newSizeY){
        this->bounds = bounds;

        sizeX = newSizeX;
        sizeY = newSizeY;

        UpdateFixedBounds();
    }

    Widget::~Widget(){}

    void Widget::Paint(surface_t* surface){}

    void Widget::OnMouseDown(vector2i_t mousePos){}

    void Widget::OnMouseUp(vector2i_t mousePos){}

    void Widget::OnHover(vector2i_t mousePos){}

    void Widget::OnMouseMove(vector2i_t mousePos) {}

    void Widget::OnKeyPress(int key) {}

    void Widget::UpdateFixedBounds(){
        fixedBounds.pos = bounds.pos;

        if(parent){
            fixedBounds.pos += parent->GetFixedBounds().pos;

            if(sizeX == LayoutSize::Stretch){
                fixedBounds.width = parent->bounds.width - bounds.width - bounds.x;
            } else {
                fixedBounds.width = bounds.width;
            }
            
            if(sizeY == LayoutSize::Stretch){
                fixedBounds.height = parent->bounds.height - bounds.height - bounds.y;
            } else {
                fixedBounds.height = bounds.height;
            }
        } else {
            fixedBounds.size = bounds.size;
        }
    }

    //////////////////////////
    // Contianer
    //////////////////////////
    Container::Container(rect_t bounds) : Widget(bounds) {

    }

    void Container::AddWidget(Widget* w){
        children.push_back(w);

        w->SetParent(this);
    }

    void Container::Paint(surface_t* surface){
        if(background.a == 255) Graphics::DrawRect(fixedBounds, background, surface);

        for(Widget* w : children){
            w->Paint(surface);
        }
    }

    void Container::OnMouseDown(vector2i_t mousePos){
        for(Widget* w : children){
            if(Graphics::PointInRect(w->GetFixedBounds(), mousePos)){
                active = w;
                w->OnMouseDown(mousePos);
                break;
            }
        }
    }

    void Container::OnMouseUp(vector2i_t mousePos){
        for(Widget* w : children){
            if(Graphics::PointInRect(w->GetFixedBounds(), mousePos)){
                w->OnMouseUp(mousePos);
                break;
            }
        }
    }

    void Container::OnMouseMove(vector2i_t mousePos){
        if(active){
            active->OnMouseMove(mousePos);
        }
    }

    void Container::OnKeyPress(int key){
        if(active) {
            active->OnKeyPress(key);
        }
    }

    void Container::UpdateFixedBounds(){
        for(Widget* w : children){
            w->UpdateFixedBounds();
        }

        Widget::UpdateFixedBounds();
    }
    
    //////////////////////////
    // Button
    //////////////////////////
    Button::Button(const char* _label, rect_t _bounds) : Widget(_bounds) {
        strcpy(label, _label);
        labelLength = Graphics::GetTextLength(label);
    }

    void Button::DrawButtonBorders(surface_t* surface, bool white){
        vector2i_t btnPos = fixedBounds.pos;
        rect_t bounds = fixedBounds;

        if(white){
            Graphics::DrawRect(bounds.pos.x+1, btnPos.y, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x, btnPos.y + 1, 1, bounds.size.y - 2, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x + bounds.size.x - 1, btnPos.y + 1, 1, bounds.size.y-2, 250, 250, 250, surface);
        } else {
            Graphics::DrawRect(btnPos.x+1, btnPos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x, btnPos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x + bounds.size.x - 1, btnPos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
        }
    }

    void Button::DrawButtonLabel(surface_t* surface, bool white){
        rgba_colour_t colour;
        vector2i_t btnPos = fixedBounds.pos;

        if(white){
            colour = {250, 250, 250, 255};
        } else {
            colour = {0, 0, 0, 255};
        }

        if(labelAlignment = TextAlignment::Centre){
            Graphics::DrawString(label, btnPos.x + (fixedBounds.size.x / 2) - (labelLength / 2), btnPos.y + (bounds.size.y / 2 - 8), colour.r, colour.g, colour.b, surface);
        } else {
            Graphics::DrawString(label, btnPos.x + 2, btnPos.y + (fixedBounds.height / 2 - 8), colour.r, colour.g, colour.b, surface);
        }
    }

    void Button::Paint(surface_t* surface){
        vector2i_t btnPos = fixedBounds.pos;
        rect_t bounds = fixedBounds;

        if(pressed){
            Graphics::DrawRect(btnPos.x+1, btnPos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224/1.1, 224/1.1, 219/1.1, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);

            DrawButtonBorders(surface, false);
        } else {
            switch(style){
                case 1: // Blue
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{50,50,150,255},{45,45,130,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                case 2: // Red
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{180,60,60,255},{160,55,55,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                case 3: // Yellow
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{200,160,50,255},{180,140,45,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                default:
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{250,250,250,255},{235,235,230,255},surface);
                    DrawButtonBorders(surface, false);
                    if(drawText) DrawButtonLabel(surface, false);
                    break;
            }
            
        }
    }

    void Button::OnMouseDown(vector2i_t mousePos){
        pressed = true;
    }

    void Button::OnMouseUp(vector2i_t mousePos){
        if(OnPress) OnPress(this);

        pressed = false;
    }

    //////////////////////////
    // Bitmap
    //////////////////////////

    Bitmap::Bitmap(rect_t _bounds) : Widget(_bounds) {
        surface.buffer = (uint8_t*)malloc(fixedBounds.size.x*fixedBounds.size.y*4);
        surface.width = fixedBounds.size.x;
        surface.height = fixedBounds.size.y;
    }
    
    Bitmap::Bitmap(rect_t _bounds, surface_t* surf){
        bounds = {bounds.pos, (vector2i_t){surf->width, surf->height}};
        surface = *surf;
    }

    void Bitmap::Paint(surface_t* surface){
        Graphics::surfacecpy(surface, &this->surface, fixedBounds.pos);
    }

    //////////////////////////
    // Label
    //////////////////////////
    Label::Label(const char* _label, rect_t _bounds) : Widget(_bounds) {
        label = _label;
    }

    void Label::Paint(surface_t* surface){
        Graphics::DrawString(label.c_str(), fixedBounds.pos.x, fixedBounds.pos.y, 0, 0, 0, surface);
    }

    //////////////////////////
    // Scroll Bar
    //////////////////////////
    void ScrollBar::ResetScrollBar(int displayHeight, int areaHeight){
        double scrollDisplayRange = ((double)areaHeight) / displayHeight; // Essentially how many 'displayHeight's in areaHeight
        scrollIncrement = ceil(scrollDisplayRange);
        scrollBar.size.y = displayHeight / scrollDisplayRange;
        scrollBar.pos.y = 0;
        scrollPos = 0;
        height = displayHeight;
    }

    void ScrollBar::Paint(surface_t* surface, vector2i_t offset, int width){
        Graphics::DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
        if(pressed) 
            Graphics::DrawRect(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y, 224/1.1, 224/1.1, 219/1.1, surface);
        else 
            Graphics::DrawGradientVertical(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y,{250,250,250,255},{235,235,230,255},surface);
    }

    void ScrollBar::OnMouseDownRelative(vector2i_t mousePos){
        if(mousePos.y > scrollBar.pos.y && mousePos.y < scrollBar.pos.y + scrollBar.size.y){
            pressed = true;
            pressOffset = mousePos.y - scrollBar.pos.y;
        }
    }

    void ScrollBar::OnMouseMoveRelative(vector2i_t relativePosition){
        if(pressed){
            scrollBar.pos.y = relativePosition.y - pressOffset;
            if(scrollBar.pos.y + scrollBar.size.y > height) scrollBar.pos.y = height - scrollBar.size.y;
            if(scrollBar.pos.y < 0) scrollBar.pos.y = 0;
            scrollPos = scrollBar.pos.y * scrollIncrement;
        }
    }

    void ScrollBarHorizontal::ResetScrollBar(int displayWidth, int areaWidth){
        double scrollDisplayRange = ((double)areaWidth) / displayWidth; // Essentially how many 'displayHeight's in areaHeight
        scrollIncrement = ceil(scrollDisplayRange);
        scrollBar.size.x = displayWidth / scrollDisplayRange;
        scrollBar.pos.x = 0;
        scrollPos = 0;
        width = displayWidth;
    }

    void ScrollBarHorizontal::Paint(surface_t* surface, vector2i_t offset, int height){
        Graphics::DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
        if(pressed) 
            Graphics::DrawRect(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height, 224/1.1, 224/1.1, 219/1.1, surface);
        else 
            Graphics::DrawGradient(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height,{250,250,250,255},{235,235,230,255},surface);
    }

    void ScrollBarHorizontal::OnMouseDownRelative(vector2i_t mousePos){
        if(mousePos.x > scrollBar.pos.x && mousePos.x < scrollBar.pos.x + scrollBar.size.x){
            pressed = true;
            pressOffset = mousePos.x - scrollBar.pos.x;
        }
    }

    void ScrollBarHorizontal::OnMouseMoveRelative(vector2i_t relativePosition){
        if(pressed){
            scrollBar.pos.x = relativePosition.x - pressOffset;
            if(scrollBar.pos.x + scrollBar.size.x > width) scrollBar.pos.x = width - scrollBar.size.x;
            if(scrollBar.pos.x < 0) scrollBar.pos.x = 0;
            scrollPos = scrollBar.pos.x * scrollIncrement;
        }
    }
}
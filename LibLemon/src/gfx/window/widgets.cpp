#include <gfx/window/widgets.h>

#include <string.h>
#include <stdlib.h>

#include <math.h>

#define BUTTON_COLOUR_OUTLINE_DEFAULT

void Widget::Paint(surface_t* surface){
}

void Widget::OnMouseDown(vector2i_t mousePos){
}

void Widget::OnMouseUp(vector2i_t mousePos){
}

//////////////////////////
// Text Box
//////////////////////////
TextBox::TextBox(rect_t bounds){
    this->bounds = bounds;
}

void TextBox::Paint(surface_t* surface){
    DrawRect(bounds.pos.x, bounds.pos.y, bounds.size.x, bounds.size.y, 255, 255, 255, surface);
    int xpos = 0;
    int ypos = 0;
    for(int i = 0; i < strlen(contents); i++){
        DrawChar(contents[i], bounds.pos.x + xpos, bounds.pos.y + ypos, textColour.r, textColour.g, textColour.b, surface);
        xpos += 8;

        if((xpos > (bounds.size.x - 8 - 16) ) || (contents[i] == '\n')){
            xpos = 0;
            ypos += 12;
        }
    }
    DrawRect(xpos + 1, ypos, 2, 12, 0, 0, 0, surface);

    DrawRect(bounds.size.x - 16, 0, 16, bounds.size.y, 240, 240, 240, surface);

    DrawRect(bounds.size.x - 14, 2, 12, 12, 128, 128, 192, surface);
    DrawRect(bounds.size.x - 14, bounds.size.y - 14, 12, 12, 128, 128, 192, surface);
}

void TextBox::LoadText(char* text){
    if(strlen(text) > 256)
        realloc(contents, strlen(text));
    memset(contents,0,256);
    strcpy(contents, text);
}

void TextBox::OnMouseDown(vector2i_t mousePos){
    active = true;
}

//////////////////////////
// Button
//////////////////////////
Button::Button(char* _label, rect_t _bounds){
    labelLength = strlen(_label);
    strcpy(label, _label);

    bounds = _bounds;
}

void Button::DrawButtonBorders(surface_t* surface, bool white){
    if(white){
        DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 250, 250, 250, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 250, 250, 250, surface);
        DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 250, 250, 250, surface);
        DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 250, 250, 250, surface);
    } else {
        DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
        DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
    }
}

void Button::Paint(surface_t* surface){

    if(pressed){
        DrawRect(bounds.pos.x+1, bounds.pos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224/1.1, 224/1.1, 219/1.1, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);
        //DrawGradient(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{250,250,250,255},{235,235,230,255},surface);

        DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
        DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
    } else {
        //DrawRect(bounds.pos.x+1, bounds.pos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224, 224, 219, surface);
        //DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);
        switch(style){
            case 1: // Blue
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{50,50,150,255},{45,45,130,255},surface);
                DrawButtonBorders(surface, true);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                break;
            case 2: // Red
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{180,60,60,255},{160,55,55,255},surface);
                DrawButtonBorders(surface, true);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                break;
            case 3: // Yellow
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{200,160,50,255},{180,140,45,255},surface);
                DrawButtonBorders(surface, true);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                break;
            default:
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{250,250,250,255},{235,235,230,255},surface);
                DrawButtonBorders(surface, false);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 0, 0, 0, surface);
                break;
        }
        
    }
}

void Button::OnMouseDown(vector2i_t mousePos){
    pressed = true;
}

void Button::OnMouseUp(vector2i_t mousePos){
    pressed = false;
}

//////////////////////////
// Bitmap
//////////////////////////
Bitmap::Bitmap(rect_t _bounds){
    bounds = _bounds;
    surface.buffer = (uint8_t*)malloc(bounds.size.x*bounds.size.y*4);
    surface.width = bounds.size.x;
    surface.height = bounds.size.y;
}

void Bitmap::Paint(surface_t* surface){
    for(int i = 0; i < this->surface.height; i++){
        memcpy(surface->buffer + (i + bounds.pos.y) * surface->width * 4 + bounds.pos.x * 4, this->surface.buffer + bounds.size.x * i * 4, bounds.size.x * 4);
    }
}

//////////////////////////
// Label
//////////////////////////
Label::Label(char* _label, rect_t _bounds){
    labelLength = strlen(_label);
    label = (char*)malloc(labelLength);
    strcpy(label, _label);

    bounds = _bounds;
}

void Label::Paint(surface_t* surface){
    int xpos = 0;
    int ypos = 0;
    for(int i = 0; i < strlen(label); i++){
        DrawChar(label[i], bounds.pos.x + xpos, bounds.pos.y + ypos, 0, 0, 0, surface);
        xpos += 8;

        if((xpos > (bounds.size.x - 8) ) || (label[i] == '\n')){
            xpos = 0;
            ypos += 12;
        }
    }
}

//////////////////////////
// Scroll View
//////////////////////////
ScrollView::ScrollView(rect_t _bounds){
    bounds = _bounds;
}

void ScrollView::Paint(surface_t* surface){
    for(int i = 0; i < contents.get_length(); i++){
        rect_t _bounds = contents[i]->bounds;
        contents[i]->bounds = {_bounds.pos + _bounds.pos, _bounds.size};

        contents[i]->Paint(surface);
        contents[i]->bounds = _bounds;
    }

    DrawRect(bounds.pos.x + bounds.size.x - 24, bounds.pos.y, 24, bounds.size.y, {120, 120, 110, 255}, surface);
}

void ScrollView::OnMouseDown(vector2i_t mousePos){

}

void ScrollView::OnMouseUp(vector2i_t mousePos){
    
}

//////////////////////////
// ListView
//////////////////////////
ListItem::ListItem(char* _text){
    text = (char*)malloc(strlen(_text) + 1);//new char[strlen(_text) + 1];
    strcpy(text, _text);
}

ListView::ListView(rect_t _bounds, int itemHeight){
    bounds = _bounds;
    this->itemHeight = itemHeight;
}

ListView::~ListView(){
}

void ListView::ResetScrollBar(){
    double iCount = contents.get_length();
    
    scrollMax = floor((iCount * itemHeight - bounds.size.y) / 2);
    scrollIncrementPixels = (bounds.size.y / 2) / floor((iCount * itemHeight - bounds.size.y) / 2);
    scrollBarHeight = bounds.size.y - scrollMax;
    //scrollMax
    //scrollMaxPixels = bounds.size.y / 2;

    scrollPos = 0;
}

void ListView::Paint(surface_t* surface){
    //DrawRect(bounds.pos.x, bounds.pos.y, bounds.size.x - 20, bounds.size.y, {250, 250, 250,}, surface);

    for(int i = 0; i < contents.get_length(); i++){
        ListItem* item = contents.get_at(i);
        if(selected == i){
            DrawRect(2,i*itemHeight + 2 - scrollPos, bounds.size.x - 24, itemHeight - 4, {165,/*24*/32,24}, surface);
        }
        DrawString(item->text, bounds.pos.x + 4, bounds.pos.y + i * itemHeight + (itemHeight / 2) - 6 - (scrollPos * scrollIncrementPixels * 2), 0, 0, 0, surface);
    }

    DrawRect(bounds.pos.x + bounds.size.x - 20, bounds.pos.y, 20, bounds.size.y, {120, 120, 110}, surface); 
    
    DrawGradientVertical(bounds.pos.x + 1 + bounds.size.x - 20, bounds.pos.y + 1 + scrollPos, 18, scrollBarHeight,{250,250,250,255},{235,235,230,255},surface);
}

void ListView::OnMouseDown(vector2i_t mousePos){
    if(mousePos.x > bounds.pos.x + bounds.size.x - 20){
        drag = true;
        dragPos = mousePos;

        return;
    }
    selected = floor(((double)mousePos.y + scrollPos) / itemHeight);

    if(selected >= contents.get_length()) selected = contents.get_length();
}

void ListView::OnMouseUp(vector2i_t mousePos){
    if(drag){
        drag = false;
        //scrollPos = mousePos.y;//+= mousePos.y - dragPos.y;
        if(!scrollPos) scrollPos = scrollMax;
        else scrollPos = 0;
        if(scrollPos < 0) scrollPos = 0;
        else if (scrollPos > scrollMax) scrollPos = scrollMax;
    }
}

void ListItem::OnMouseDown(vector2i_t mousePos){
}

void ListItem::OnMouseUp(vector2i_t mousePos){
    
}
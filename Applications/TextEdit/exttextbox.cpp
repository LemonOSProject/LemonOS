#include "exttextbox.h"

#include <math.h>

ExtendedTextBox::ExtendedTextBox(rect_t bounds) : Lemon::GUI::TextBox({{bounds.pos.x + 40, bounds.pos.y}, {bounds.size.x - 40, bounds.size.y}}){
    textBoxBounds = bounds;
    textBoxBounds.pos.x += 40;
    textBoxBounds.size.x -= 40;
    normalBounds = bounds;
}

void ExtendedTextBox::Paint(surface_t* surface){
    char num[10];
    for(int i = floor(((double)sBar.scrollPos) / (font->height + lineSpacing)); i < contents.size(); i++){
        int yPos = i * (lineSpacing + font->height) - sBar.scrollPos;
        Lemon::Graphics::DrawRect(0, 40, yPos, (lineSpacing + font->height), 160, 160, 160, surface);
        sprintf(num, "%d", i);
        int textSz = Lemon::Graphics::GetTextLength(num);
        Lemon::Graphics::DrawString(num, 20 - textSz / 2, yPos + lineSpacing / 2, 30, 30, 30, surface);
    }
    bounds = textBoxBounds;
    TextBox::Paint(surface);
    bounds = normalBounds;
}

void ExtendedTextBox::OnMouseDown(vector2i_t mousePos){
    if((mousePos.x - bounds.pos.x) > 40){
        bounds = textBoxBounds;
        TextBox::OnMouseDown(mousePos);
        bounds = normalBounds;
    }
}

void ExtendedTextBox::OnMouseUp(vector2i_t mousePos){
    if((mousePos.x - bounds.pos.x) > 40){
        bounds = textBoxBounds;
        TextBox::OnMouseUp(mousePos);
        bounds = normalBounds;
    }
}

void ExtendedTextBox::OnMouseMove(vector2i_t mousePos){
    if((mousePos.x - bounds.pos.x) > 40){
        bounds = textBoxBounds;
        TextBox::OnMouseMove(mousePos);
        bounds = normalBounds;
    }
}
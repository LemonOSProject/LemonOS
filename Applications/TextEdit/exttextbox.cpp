#include "exttextbox.h"

#include <math.h>

#define LINE_NUM_PANEL_WIDTH 40

ExtendedTextBox::ExtendedTextBox(rect_t bounds) : Lemon::GUI::TextBox(bounds, true){
    textBoxBounds = bounds;
    textBoxBounds.pos.x += LINE_NUM_PANEL_WIDTH;
    textBoxBounds.size.x -= LINE_NUM_PANEL_WIDTH;
    normalBounds = bounds;
}

void ExtendedTextBox::Paint(surface_t* surface){
    char num[10];
    for(int i = floor(((double)sBar.scrollPos) / (font->height + lineSpacing)); i < static_cast<int>(contents.size()); i++){
        int yPos = fixedBounds.y + i * (lineSpacing + font->height) - sBar.scrollPos;
        sprintf(num, "%d", i);
        int textSz = Lemon::Graphics::GetTextLength(num);
        Lemon::Graphics::DrawString(num, fixedBounds.pos.x + (LINE_NUM_PANEL_WIDTH / 2) - textSz / 2, yPos + lineSpacing / 2, Lemon::colours[Lemon::Colour::Text], surface);
    }
    fixedBounds = textBoxBounds;
    TextBox::Paint(surface);
    fixedBounds = normalBounds;
}

void ExtendedTextBox::OnMouseDown(vector2i_t mousePos){
    if((mousePos.x - fixedBounds.pos.x) > LINE_NUM_PANEL_WIDTH){
        fixedBounds = textBoxBounds;
        TextBox::OnMouseDown(mousePos);
        fixedBounds = normalBounds;
    }
}

void ExtendedTextBox::OnMouseUp(vector2i_t mousePos){
    if((mousePos.x - fixedBounds.pos.x) > LINE_NUM_PANEL_WIDTH){
        fixedBounds = textBoxBounds;
        TextBox::OnMouseUp(mousePos);
        fixedBounds = normalBounds;
    }
}

void ExtendedTextBox::OnMouseMove(vector2i_t mousePos){
    if((mousePos.x - fixedBounds.pos.x) > LINE_NUM_PANEL_WIDTH){
        fixedBounds = textBoxBounds;
        TextBox::OnMouseMove(mousePos);
        fixedBounds = normalBounds;
    }
}

void ExtendedTextBox::UpdateFixedBounds(){
    Widget::UpdateFixedBounds();

    textBoxBounds = fixedBounds;
    textBoxBounds.pos.x += LINE_NUM_PANEL_WIDTH;
    textBoxBounds.size.x -= LINE_NUM_PANEL_WIDTH;
    normalBounds = fixedBounds;

    ResetScrollBar();
}
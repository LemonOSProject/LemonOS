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
    for(int i = (sBar.scrollPos) / (font->lineHeight); i < static_cast<int>(contents.size()) && i * font->lineHeight < textBoxBounds.height + textBoxBounds.y; i++){
        int yPos = fixedBounds.y + (i * font->lineHeight) - sBar.scrollPos;
        snprintf(num, 10, "%d", i + 1);
        int textSz = Lemon::Graphics::GetTextLength(num);
        Lemon::Graphics::DrawString(num, fixedBounds.pos.x + (LINE_NUM_PANEL_WIDTH / 2) - textSz / 2, yPos + lineSpacing / 2, Lemon::GUI::Theme::Current().ColourText(), surface, {fixedBounds.pos, {fixedBounds.width, textBoxBounds.height}});
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
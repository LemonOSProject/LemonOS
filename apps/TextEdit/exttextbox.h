#pragma once

#include <Lemon/GUI/Widgets.h>

class ExtendedTextBox : public Lemon::GUI::TextBox {
    rect_t textBoxBounds;
    rect_t normalBounds;
public:
    ExtendedTextBox(rect_t bounds);
    void Paint(surface_t* surface);
    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);
    void OnMouseMove(vector2i_t mousePos);
    void UpdateFixedBounds();
};
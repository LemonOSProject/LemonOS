#pragma once

#include <stdint.h>
#include <gfx/surface.h>
#include <gfx/window/widgets.h>


class Brush{
public:
    surface_t data;

    void Paint(int x, int y, uint8_t r, uint8_t g, uint8_t b, double scale, class Canvas* canvas);
};

class Canvas : public Bitmap{
protected:
    vector2i_t lastMousePos;

    void DragBrush(vector2i_t a, vector2i_t b);

    ScrollBar sBarVert;
    ScrollBarHorizontal sBarHor;
public:
    Brush* currentBrush;

    rgba_colour_t colour;
    double brushScale = 1;

    bool pressed = false;

    Canvas(rect_t bounds, vector2i_t canvasSize);
    void DrawText(char* text, int size);

    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);
    void OnMouseMove(vector2i_t mousePos);
    
    void Paint(surface_t* surface);

    void ResetScrollbars();
};

class Colour : public Widget {
    public:
        rgba_colour_t colour;
        Colour(rect_t rect){
            bounds = rect;
        }
        void Paint(surface_t* surface);
    void OnMouseDown(vector2i_t mousePos);
};
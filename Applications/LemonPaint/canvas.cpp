#include "paint.h"

#include <math.h>

Canvas::Canvas(rect_t _bounds, vector2i_t canvasSize) {
    bounds = _bounds;

    surface.buffer = (uint8_t*)malloc(canvasSize.x*canvasSize.y*4);
    surface.width = canvasSize.x;
    surface.height = canvasSize.y;

    sBarVert.ResetScrollBar(bounds.size.y - 16, canvasSize.y);
    sBarHor.ResetScrollBar(bounds.size.x - 16, canvasSize.x);
}

void Canvas::DragBrush(vector2i_t a, vector2i_t b){
    double xDist = b.x - a.x;
    double yDist = b.y - a.y;

    double _x = a.x;
    double _y = a.y;

    int xsign = (_x > 0) - (_x < 0);

    if(abs(xDist) >= abs(yDist)){
        double gradient = xDist ? (yDist ? (yDist / xDist) : 0) : yDist;
        for(int i = 0; i < abs(xDist); i++, _x += xsign, _y += gradient){
            currentBrush->Paint(_x, _y, colour.r, colour.g, colour.b, brushScale, this);
        }
    } else {
        double gradient = yDist ? (xDist ? (xDist / yDist) : 0) : xDist;
        for(int i = 0; i < abs(yDist); i++, _y += xsign, _x += gradient){
            currentBrush->Paint(_x, _y, colour.r, colour.g, colour.b, brushScale, this);
        }
    }
}

void Canvas::OnMouseDown(vector2i_t mousePos){
    mousePos.x -= bounds.pos.x;
    mousePos.y -= bounds.pos.y;

    if(mousePos.y > bounds.size.y - 16){
        sBarHor.OnMouseDownRelative({mousePos.y - bounds.size.y + 16, mousePos.x});
        return;
    }
    
    if(mousePos.x > bounds.size.x - 16){
        sBarVert.OnMouseDownRelative({mousePos.y, mousePos.x - bounds.size.x + 16});
        return;
    }

    if(!pressed){
        lastMousePos = mousePos;
    }

    pressed = true;

    DragBrush(lastMousePos, mousePos);

    currentBrush->Paint(mousePos.x, mousePos.y, colour.r, colour.g, colour.b, brushScale, this);

    lastMousePos = mousePos;
}

void Canvas::OnMouseUp(vector2i_t mousePos){
    mousePos.x -= bounds.pos.x;
    mousePos.y -= bounds.pos.y;

    sBarVert.pressed = false;
    sBarHor.pressed = false;

    if(pressed) 
        DragBrush(lastMousePos, mousePos);

    pressed = false;
}

void Canvas::OnMouseMove(vector2i_t mousePos){
    mousePos.x -= bounds.pos.x;
    mousePos.y -= bounds.pos.y;

    if(sBarVert.pressed){
        sBarVert.OnMouseMoveRelative(mousePos);
    } else if(sBarHor.pressed){
        sBarHor.OnMouseMoveRelative(mousePos);
    } else if(pressed){
        DragBrush(lastMousePos, mousePos);

        currentBrush->Paint(mousePos.x, mousePos.y, colour.r, colour.g, colour.b, brushScale, this);
    }
    
    lastMousePos = mousePos;
}

void Canvas::Paint(surface_t* surface){
    Lemon::Graphics::surfacecpy(surface, &this->surface, bounds.pos, {{sBarHor.scrollPos, sBarVert.scrollPos},{bounds.size - (vector2i_t){16, 16}}});
    sBarVert.Paint(surface, {bounds.pos + (vector2i_t){bounds.size.x - 16, 0}});
    sBarHor.Paint(surface, {bounds.pos + (vector2i_t){0, bounds.size.y - 16}});
}

void Canvas::ResetScrollbars(){
    sBarVert.ResetScrollBar(bounds.size.y - 16, surface.height);
    sBarHor.ResetScrollBar(bounds.size.x - 16, surface.width);
}
#pragma once

#include <gfx/graphics.h>

#include <string.h>
#include <stddef.h>
#include <list.h>

class Widget {
public:
    rect_t bounds;

    virtual void Paint(surface_t* surface);
    virtual void OnMouseDown();
    virtual void OnMouseUp();
};

class TextBox : public Widget{
public:
    bool editable;
    bool multiline;
    bool active;
    char contents[256];
    size_t bufferSize;

    TextBox(rect_t bounds);

    void Paint(surface_t* surface);
    void LoadText(char* text);

    void OnMouseDown();
    void OnKeyDown(char c);

    rgba_colour_t textColour = {0,0,0,225};
};

class Button : public Widget{
public:
    bool active;
    bool pressed;
    char label[64];
    int labelLength;
    int style; // 0 - Normal, 1 - Blue, 2 - Red, 3 - Yellow

    bool state;

    Button(char* _label, rect_t _bounds);

    void Paint(surface_t* surface);
    void OnMouseDown();
    void OnMouseUp();
    void DrawButtonBorders(surface_t* surface, bool white);
};

class BitmapButton : public Button{
public:
    surface_t surface;

    BitmapButton(char* _label, rect_t _bounds);
    void Paint(surface_t* surface);
};

class Bitmap : public Widget{
public:
    surface_t surface;

    Bitmap(rect_t _bounds);
    void Paint(surface_t* surface);
};

class Label : public Widget{
public:
    char* label;
    int labelLength;
    Label(char* _label, rect_t _bounds);

    void Paint(surface_t* surface);
};

class ContextMenuItem : public Widget{
    char* label;
    int labelLength;
    void Paint(surface_t* surface);
};

class ContextMenu : public Widget{
public:
    List<ContextMenuItem> items;

    void Paint(surface_t* surface);
};

class ScrollContainer : public Widget{
public:
    List<Widget> widgets;

    ScrollContainer(rect_t bounds);
    void Paint(surface_t* surface);
};
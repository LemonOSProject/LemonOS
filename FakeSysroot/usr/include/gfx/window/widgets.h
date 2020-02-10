#pragma once

#include <gfx/graphics.h>

#include <string.h>
#include <stddef.h>
#include <list.h>

class Widget {
public:
    rect_t bounds;

    virtual void Paint(surface_t* surface);
    virtual void OnMouseDown(vector2i_t mousePos);
    virtual void OnMouseUp(vector2i_t mousePos);
};

class ScrollView : public Widget {
public:
    int scrollPos;
    int scrollMax;

    int scrollBarHeight;

    List<Widget*> contents;

    ScrollView(rect_t bounds);

    void Paint(surface_t* surface);
    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);
};

class ListItem : public Widget {
public:
    ListItem(char* _text);
    char* text;
    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);
};

class ListView : public Widget{
protected:
    int itemHeight;
    int scrollMax;
    int scrollIncrementPixels;
    int scrollMaxPixels;
    int scrollBarHeight;

    bool drag = false;
    vector2i_t dragPos;

    surface_t buffer;
public:
    int scrollPos;
    int selected = 0;
    List<ListItem*> contents;

    ListView(rect_t bounds, int itemHeight = 20);

    void ResetScrollBar();

    void Paint(surface_t* surface);
    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);

    ~ListView();
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

    void OnMouseDown(vector2i_t mousePos);
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
    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);
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
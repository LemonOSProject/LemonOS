#include <gfx/graphics.h>

#include <string.h>

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
    char** contents;

    TextBox(rect_t bounds);

    void Paint(surface_t* surface);
};

class Button : public Widget{
public:
    bool active;
    bool pressed;
    char label[64];
    int labelLength;

    bool state;

    Button(char* _label, rect_t _bounds);

    void Paint(surface_t* surface);
    void OnMouseDown();
    void OnMouseUp();
};
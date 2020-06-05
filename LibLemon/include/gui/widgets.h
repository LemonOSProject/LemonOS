#pragma once

#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <list.h>

#include <vector>
#include <string>

namespace Lemon::GUI {
    enum LayoutSize {
        Fixed,      // Fixed Size in Pixels
        Stretch,   // Size is a offset from the edge of the container
    };

    enum TextAlignment {
        Left,
        Centre,
        Right,
    };

    enum WidgetAlignment {
        WAlignLeft,
        WAlignCentre,
        WAlignRight,
    };

    class Widget {
    protected:
        Widget* parent = nullptr;

        short layoutSizeX = LayoutSize::Fixed;
        short layoutSizeY = LayoutSize::Fixed;

        rect_t bounds;
        rect_t fixedBounds;

        LayoutSize sizeX;
        LayoutSize sizeY;

        WidgetAlignment align = WAlignLeft;
    public:
        Widget();
        Widget(rect_t bounds, LayoutSize newSizeX = LayoutSize::Fixed, LayoutSize newSizeY = LayoutSize::Fixed);
        virtual ~Widget();

        virtual void SetParent(Widget* newParent) { parent = newParent; UpdateFixedBounds(); };

        virtual void SetLayout(LayoutSize newSizeX, LayoutSize newSizeY, WidgetAlignment newAlign){ sizeX = newSizeX; sizeY = newSizeY; align = newAlign; UpdateFixedBounds(); };

        virtual void Paint(surface_t* surface);

        virtual void OnMouseDown(vector2i_t mousePos);
        virtual void OnMouseUp(vector2i_t mousePos);
        virtual void OnMouseMove(vector2i_t mousePos);
        virtual void OnKeyPress(int key);
        virtual void OnHover(vector2i_t mousePos);

        virtual void UpdateFixedBounds();

        rect_t GetBounds() { return bounds; }
        rect_t GetFixedBounds() { return fixedBounds; }

        virtual void SetBounds(rect_t bounds) { this->bounds = bounds; UpdateFixedBounds(); };
    };

    class Container : public Widget {
    protected:
        std::vector<Widget*> children;

        Widget* active;

    public:
        rgba_colour_t background = {190, 190, 180, 255};

        Container(rect_t bounds);

        void AddWidget(Widget* w);

        void Paint(surface_t* surface);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnKeyPress(int key);

        void UpdateFixedBounds();
    };

    class ScrollBar { /* Not a widget, but is to be used in widgets*/
    protected:
        int scrollIncrement;
        int pressOffset;
        int height;
    public:
        bool pressed;

        rect_t scrollBar;

        int scrollPos = 0;

        void ResetScrollBar(int displayHeight /* Region that can be displayed at one time */, int areaHeight /* Total Scroll Area*/);
        void Paint(surface_t* surface, vector2i_t offset, int width = 16);

        void OnMouseDownRelative(vector2i_t relativePosition); // Relative to the position of the scroll bar.
        void OnMouseMoveRelative(vector2i_t relativePosition);
    };

    class ScrollBarHorizontal { /* Not a widget, but is to be used in widgets*/
    protected:
        int scrollIncrement;
        int pressOffset;
        int width;
    public:
        bool pressed;

        rect_t scrollBar;

        int scrollPos = 0;

        void ResetScrollBar(int displayWidth /* Region that can be displayed at one time */, int areaWidth /* Total Scroll Area*/);
        void Paint(surface_t* surface, vector2i_t offset, int height = 16);

        void OnMouseDownRelative(vector2i_t relativePosition); // Relative to the position of the scroll bar.
        void OnMouseMoveRelative(vector2i_t relativePosition);
    };

    class Button : public Widget{
    protected:
        bool drawText = true;

        TextAlignment labelAlignment = TextAlignment::Centre;

        void DrawButtonBorders(surface_t* surface, bool white);
        void DrawButtonLabel(surface_t* surface, bool white);
    public:
        bool active;
        bool pressed;
        char label[64];
        int labelLength;
        int style; // 0 - Normal, 1 - Blue, 2 - Red, 3 - Yellow

        bool state;

        Button(const char* _label, rect_t _bounds);

        void Paint(surface_t* surface);
        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);

        void (*OnPress)(Button*) = nullptr;
    };

    class Bitmap : public Widget{
    public:
        surface_t surface;

        Bitmap(rect_t _bounds);
        Bitmap(rect_t _bounds, surface_t* surf);
        void Paint(surface_t* surface);
    };

    class Label : public Widget{
    public:
        std::string label;
        Label(const char* _label, rect_t _bounds);

        void Paint(surface_t* surface);
    };

    class TextBox : public Widget{
    protected:
        ScrollBar sBar;
    public:
        bool editable;
        bool multiline;
        bool active;
        std::vector<std::string> contents;
        int lineCount;
        int lineSpacing = 3;
        size_t bufferSize;
        vector2i_t cursorPos = {0, 0};
        Graphics::Font* font;

        TextBox(rect_t bounds, bool multiline);

        void Paint(surface_t* surface);
        void LoadText(char* text);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnKeyPress(int key);

        void ResetScrollBar();

        rgba_colour_t textColour = {0,0,0,255};
    };
}
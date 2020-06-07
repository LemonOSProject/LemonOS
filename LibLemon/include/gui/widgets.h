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
        Widget* active = nullptr; // Only applies to containers, etc. is so widgets know whether they are active or not

        Widget();
        Widget(rect_t bounds, LayoutSize newSizeX = LayoutSize::Fixed, LayoutSize newSizeY = LayoutSize::Fixed);
        virtual ~Widget();

        virtual void SetParent(Widget* newParent) { parent = newParent; UpdateFixedBounds(); };
        Widget* GetParent() { return parent; }

        virtual void SetLayout(LayoutSize newSizeX, LayoutSize newSizeY, WidgetAlignment newAlign){ sizeX = newSizeX; sizeY = newSizeY; align = newAlign; UpdateFixedBounds(); };

        virtual void Paint(surface_t* surface);

        virtual void OnMouseDown(vector2i_t mousePos);
        virtual void OnMouseUp(vector2i_t mousePos);
        virtual void OnMouseMove(vector2i_t mousePos);
        virtual void OnDoubleClick(vector2i_t mousePos);
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

    public:
        rgba_colour_t background = {190, 190, 180, 255};

        Container(rect_t bounds);

        void AddWidget(Widget* w);

        void Paint(surface_t* surface);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnDoubleClick(vector2i_t mousePos);
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
        bool editable =  true;
        bool multiline = false;
        bool active;
        std::vector<std::string> contents;
        int lineCount;
        int lineSpacing = 3;
        size_t bufferSize;
        vector2i_t cursorPos = {0, 0};
        Graphics::Font* font;

        TextBox(rect_t bounds, bool multiline);

        void Paint(surface_t* surface);
        void LoadText(const char* text);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnKeyPress(int key);

        void ResetScrollBar();

        rgba_colour_t textColour = {0,0,0,255};
        
        void (*OnSubmit)(TextBox*) = nullptr;
    };

    class ListItem{
        public:
        std::vector<std::string> details;
    };

    class ListColumn{
        public:
        std::string name;
        int displayWidth;
    };

    class ListView : public Widget{
        ListColumn primaryColumn;
        std::vector<ListColumn> columns;
        std::vector<ListItem> items;

        int selected = 0;
        int itemHeight = 20;
        int columnDisplayHeight = 20;

        bool showScrollBar = false;

        ScrollBar sBar;

        Graphics::Font* font;

        void ResetScrollBar();
    public:
        ListView(rect_t bounds);
        ~ListView();

        void Paint(surface_t* surface);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnDoubleClick(vector2i_t mousePos);
        void OnKeyPress(int key);

        void AddColumn(ListColumn& column);
        int AddItem(ListItem& item);
        void ClearItems();

        void UpdateFixedBounds();

        void(*OnSubmit)(ListItem&, ListView*) = nullptr;
    };
    
    class FileView : public Container{
    protected:
        int pathBoxHeight = 20;
        int sidepanelWidth = 120;
        int currentDir;
        char** filePointer;

        void(*OnFileOpened)(char*, FileView*) = nullptr;

        ListView* fileList;
        TextBox* pathBox;

        Widget* active;

        ListColumn nameCol, sizeCol;
        
    public:
        static surface_t icons;

        std::string currentPath;
        FileView(rect_t bounds, char* path, void(*_OnFileOpened)(char*, FileView*));
        
        void Refresh();

        void OnSubmit(ListItem& item, ListView* list);
        void OnTextSubmit();
        static void OnListSubmit(ListItem& item, ListView* list);
        static void OnTextBoxSubmit(TextBox* textBox);
    };
}
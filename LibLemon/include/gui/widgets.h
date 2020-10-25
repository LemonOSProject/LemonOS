#pragma once

#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/ctxentry.h>
#include <gui/colours.h>
#include <list.h>

#include <vector>
#include <string>

namespace Lemon::GUI {
    class Window;

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
        WAlignTop,
        WAlignBottom,
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
        WidgetAlignment verticalAlign = WAlignTop;
    public:
        Widget* active = nullptr; // Only applies to containers, etc. is so widgets know whether they are active or not
        Window* window = nullptr;

        Widget();
        Widget(rect_t bounds, LayoutSize newSizeX = LayoutSize::Fixed, LayoutSize newSizeY = LayoutSize::Fixed);
        virtual ~Widget();

        virtual void SetParent(Widget* newParent) { parent = newParent; UpdateFixedBounds(); };
        Widget* GetParent() { return parent; }

        virtual void SetLayout(LayoutSize newSizeX, LayoutSize newSizeY, WidgetAlignment newAlign = WAlignLeft, WidgetAlignment newAlignVert = WAlignTop){ sizeX = newSizeX; sizeY = newSizeY; align = newAlign; verticalAlign = newAlignVert; UpdateFixedBounds(); };

        virtual void Paint(surface_t* surface);

        virtual void OnMouseDown(vector2i_t mousePos);
        virtual void OnMouseUp(vector2i_t mousePos);
        virtual void OnMouseMove(vector2i_t mousePos);
        virtual void OnRightMouseDown(vector2i_t mousePos);
        virtual void OnRightMouseUp(vector2i_t mousePos);
        virtual void OnDoubleClick(vector2i_t mousePos);
        virtual void OnKeyPress(int key);
        virtual void OnHover(vector2i_t mousePos);
        virtual void OnCommand(unsigned short command);

        virtual void UpdateFixedBounds();

        rect_t GetBounds() { return bounds; }
        rect_t GetFixedBounds() { return fixedBounds; }

        virtual void SetBounds(rect_t bounds) { this->bounds = bounds; UpdateFixedBounds(); };
    };

    class Container : public Widget {
    protected:
        std::vector<Widget*> children;

    public:
        rgba_colour_t background = Lemon::colours[Lemon::Colour::Background];

        Container(rect_t bounds);

        void AddWidget(Widget* w);
        void RemoveWidget(Widget* w);

        void Paint(surface_t* surface);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnRightMouseDown(vector2i_t mousePos);
        void OnRightMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnDoubleClick(vector2i_t mousePos);
        void OnKeyPress(int key);

        void UpdateFixedBounds();
    };

    class LayoutContainer : public Container {
    protected:
        vector2i_t itemSize;
        bool isOverflowing = false;
    public:
        LayoutContainer(rect_t bounds, vector2i_t itemSize);
        
        void AddWidget(Widget* w);
        void RemoveWidget(Widget* w);

        void UpdateFixedBounds();
        bool IsOverflowing() { return isOverflowing; }
    };

    class ScrollBar { /* Not a widget, but is to be used in widgets*/
    protected:
        int scrollIncrement;
        int pressOffset;
        int height;
    public:
        bool pressed = false;

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
        bool pressed = false;

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
        
        std::string label;
        int labelLength;

        void DrawButtonBorders(surface_t* surface, bool white);
        void DrawButtonLabel(surface_t* surface, bool white);
    public:
        bool active = false;
        bool pressed = false;
        int style = 0; // 0 - Normal, 1 - Blue, 2 - Red, 3 - Yellow

        bool state = false;

        Button(const char* _label, rect_t _bounds);

        virtual void Paint(surface_t* surface);
        virtual void OnMouseDown(vector2i_t mousePos);
        virtual void OnMouseUp(vector2i_t mousePos);

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
        rgba_colour_t textColour = colours[Colour::Text];
        std::string label;
        Label(const char* _label, rect_t _bounds);

        void Paint(surface_t* surface);
    };

    enum {
        TextboxCommandCopy = 1,
        TextboxCommandCut,
        TextboxCommandPaste,
        TextboxCommandDelete,
    };

    class TextBox : public Widget{
    protected:
        ScrollBar sBar;

        std::vector<ContextMenuEntry> ctxEntries;
        bool masked = false;
    public:
        bool editable =  true;
        bool multiline = false;
        bool active = false;
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
        void OnRightMouseDown(vector2i_t mousePos);
        void OnCommand(unsigned short cmd);
        void MaskText(bool state);

        void ResetScrollBar();

        rgba_colour_t textColour = colours[Colour::Text];
        
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
        short itemHeight = 20;
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
        void(*OnSelect)(ListItem&, ListView*) = nullptr;
    };

    class GridItem{
    public:
        surface_t* icon = nullptr;
        std::string name;
    };

    class GridView : public Widget{
        std::vector<GridItem> items;

        const vector2i_t itemSize = {80, 80};

        int selected = -1;
        int itemsPerRow = 1;
        bool showScrollBar = false;

        ScrollBar sBar;

        void ResetScrollBar();
        int PosToItem(vector2i_t relPos){
            if(!items.size()) return -1;
            
            int column = 0;
            int row = 0;
            
            if(relPos.x){
                column = relPos.x / itemSize.x;
                if(column >= itemsPerRow){
                    return -1;
                }
            }

            if(relPos.y){
                row = relPos.y / itemSize.y;
                if(row > static_cast<int>(items.size()) / itemsPerRow){
                    return -1;
                }
            }

            return row * itemsPerRow + column;
        }
    public:
        GridView(rect_t bounds) : Widget(bounds){}
        ~GridView() = default;

        void Paint(surface_t* surface);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnDoubleClick(vector2i_t mousePos);
        void OnKeyPress(int key);

        int AddItem(GridItem& item);

        void ClearItems(){
            items.clear(); ResetScrollBar();
        }

        void UpdateFixedBounds();

        void(*OnSubmit)(GridItem&, GridView*) = nullptr;
        void(*OnSelect)(GridItem&, GridView*) = nullptr;
    };
    
    class FileView : public Container{
    protected:
        int pathBoxHeight = 20;
        int sidepanelWidth = 120;
        int currentDir;
        char** filePointer;

        void(*OnFileOpened)(const char*, FileView*) = nullptr;

        GridView* fileList;
        TextBox* pathBox;

        Widget* active;

        ListColumn nameCol, sizeCol;
        
    public:
        static surface_t icons;
        static surface_t folderIcon;
        static surface_t fileIcon;

        std::string currentPath;
        FileView(rect_t bounds, const char* path, void(*_OnFileOpened)(const char*, FileView*) = nullptr);
        
        void Refresh();

        void OnSubmit(std::string& path);
        static void OnListSubmit(GridItem& item, GridView* list);
        static void OnTextBoxSubmit(TextBox* textBox);

        void (*OnFileSelected)(std::string& file, FileView* fv) = nullptr;
    };

    class ScrollView : public Container{
        ScrollBar sBarVertical;
        ScrollBarHorizontal sBarHorizontal;

        rect_t scrollBounds = {0, 0, 0, 0};
    public:
        ScrollView(rect_t b) : Container(b) {}
        void Paint(surface_t* surface);
        void AddWidget(Widget* w);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);

        void UpdateFixedBounds();
    };
}
#pragma once

#include <Lemon/GUI/ContextMenu.h>
#include <Lemon/GUI/Theme.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Graphics/Surface.h>

#include <string>
#include <vector>

namespace Lemon::GUI {
class Window;
class DataModel;

enum LayoutSize {
    Fixed,   // Fixed Size in Pixels
    Stretch, // Size is a offset from the edge of the container
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
public:
    Widget();
    Widget(rect_t bounds, LayoutSize newSizeX = LayoutSize::Fixed, LayoutSize newSizeY = LayoutSize::Fixed);
    virtual ~Widget();

    inline Window* GetWindow() { return window; }
    virtual void SetWindow(Window* win) { window = win; }

    virtual void SetParent(Widget* newParent) {
        parent = newParent;
        UpdateFixedBounds();
    };
    inline Widget* GetParent() { return parent; }

    virtual void SetLayout(LayoutSize newSizeX, LayoutSize newSizeY, WidgetAlignment newAlign = WAlignLeft,
                           WidgetAlignment newAlignVert = WAlignTop);

    virtual void Paint(surface_t* surface);

    virtual void OnMouseEnter(vector2i_t mousePos);
    virtual void OnMouseExit(vector2i_t mousePos);
    virtual void OnMouseDown(vector2i_t mousePos);
    virtual void OnMouseUp(vector2i_t mousePos);
    virtual void OnMouseMove(vector2i_t mousePos);
    virtual void OnRightMouseDown(vector2i_t mousePos);
    virtual void OnRightMouseUp(vector2i_t mousePos);
    virtual void OnDoubleClick(vector2i_t mousePos);
    virtual void OnKeyPress(int key);
    virtual void OnHover(vector2i_t mousePos);
    virtual void OnCommand(unsigned short command);
    virtual void OnActive();
    virtual void OnInactive();

    virtual void UpdateFixedBounds();

    inline virtual bool IsActive() {
        assert(parent);
        return parent->active == this;
    }

    inline rect_t GetBounds() { return bounds; }
    inline rect_t GetFixedBounds() { return fixedBounds; }

    virtual void SetBounds(rect_t bounds) {
        this->bounds = bounds;
        UpdateFixedBounds();
    };

    Widget* active = nullptr; // Only applies to containers, etc. is so widgets know whether they are active or not
protected:
    Widget* parent = nullptr;
    Window* window = nullptr;

    rect_t bounds;
    rect_t fixedBounds;

    LayoutSize sizeX;
    LayoutSize sizeY;

    WidgetAlignment align = WAlignLeft;
    WidgetAlignment verticalAlign = WAlignTop;
};

class Container : public Widget {
public:
    rgba_colour_t background = Theme::Current().ColourBackground();

    Container(rect_t bounds);
    virtual ~Container();

    virtual void SetWindow(Window* w); // We need to set the windows of the children

    virtual void AddWidget(Widget* w);
    virtual void RemoveWidget(Widget* w);

    virtual void Paint(surface_t* surface);

    virtual void OnMouseEnter(vector2i_t mousePos);
    virtual void OnMouseExit(vector2i_t mousePos);
    virtual void OnMouseDown(vector2i_t mousePos);
    virtual void OnMouseUp(vector2i_t mousePos);
    virtual void OnRightMouseDown(vector2i_t mousePos);
    virtual void OnRightMouseUp(vector2i_t mousePos);
    virtual void OnMouseMove(vector2i_t mousePos);
    virtual void OnDoubleClick(vector2i_t mousePos);
    virtual void OnKeyPress(int key);

    virtual void UpdateFixedBounds();

protected:
    std::vector<Widget*> children;

    Widget* lastMousedOver = nullptr;
};

class LayoutContainer : public Container {
protected:
    vector2i_t itemSize;
    bool isOverflowing = false;

public:
    int xPadding = 2;
    int yPadding = 2;

    // Whether or not the LayoutContainer should expand the Widgets
    // to fill the width of the container
    bool xFill = false;

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

    void ResetScrollBar(int displayHeight /* Region that can be displayed at one time */,
                        int areaHeight /* Total Scroll Area*/);
    void Paint(surface_t* surface, vector2i_t offset, int width = 16);

    void ScrollTo(int pos);

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

    void ResetScrollBar(int displayWidth /* Region that can be displayed at one time */,
                        int areaWidth /* Total Scroll Area*/);
    void Paint(surface_t* surface, vector2i_t offset, int height = 16);

    void OnMouseDownRelative(vector2i_t relativePosition); // Relative to the position of the scroll bar.
    void OnMouseMoveRelative(vector2i_t relativePosition);
};

class Button : public Widget {
protected:
    bool drawText = true;

    TextAlignment labelAlignment = TextAlignment::Centre;

    std::string label;
    int labelLength;

    void DrawButtonLabel(surface_t* surface);

public:
    bool active = false;
    bool pressed = false;
    int style = 0; // 0 - Normal, 1 - Blue, 2 - Red, 3 - Yellow

    bool state = false;

    Button(const char* _label, rect_t _bounds);

    void SetLabel(const char* _label);

    virtual void Paint(surface_t* surface);
    virtual void OnMouseDown(vector2i_t mousePos);
    virtual void OnMouseUp(vector2i_t mousePos);

    void (*OnPress)(Button*) = nullptr;
};

class Bitmap : public Widget {
public:
    surface_t surface;

    Bitmap(rect_t _bounds);
    Bitmap(rect_t _bounds, surface_t* surf);
    void Paint(surface_t* surface);
};

class Image : public Widget {
public:
    Image(rect_t _bounds);
    Image(rect_t _bounds, const char* path);

    void Load(surface_t* image);
    int Load(const char* imagePath);
    inline void SetScaling(Graphics::Texture::TextureScaling scaling) { texture.SetScaling(scaling); }

    void Paint(surface_t* surface);

    void UpdateFixedBounds();

protected:
    Graphics::Texture texture;
};

class Label : public Widget {
public:
    rgba_colour_t textColour = Theme::Current().ColourTextLight();
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

class TextBox : public Widget {
protected:
    ScrollBar sBar;

    std::vector<ContextMenuEntry> ctxEntries;
    bool masked = false;

public:
    bool editable = true;
    bool multiline = false;
    bool active = false;
    std::vector<std::string> contents;
    int lineCount;
    int lineSpacing = 3;
    size_t bufferSize;
    vector2i_t cursorPos = {0, 0};
    Graphics::Font* font = Graphics::DefaultFont();

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

    rgba_colour_t textColour = Theme::Current().ColourTextLight();

    void (*OnSubmit)(TextBox*) = nullptr;
};

struct ListItem {
    std::vector<std::string> details;
};

struct ListColumn {
    std::string name;
    int displayWidth;
    bool editable;
};

class ListView : public Widget {
public:
    ListView(rect_t bounds);
    ~ListView();

    void SetModel(DataModel* model);

    void Paint(surface_t* surface);

    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);
    void OnMouseMove(vector2i_t mousePos);
    void OnDoubleClick(vector2i_t mousePos);
    void OnKeyPress(int key);
    void OnInactive();

    void UpdateFixedBounds();

    void OnEditboxSubmit();

    void (*OnEdit)(int, ListView*) = nullptr;
    void (*OnSubmit)(int, ListView*) = nullptr;
    void (*OnSelect)(int, ListView*) = nullptr;

    ///////////////////////////////////////////
    /// \brief Custom paint function
    ///
    /// \param row Row to paint
    /// \param model Data model reference
    /// \param mask Mask to paint (position of mask is screen position for paint)
    void (*CustomPaint)(int, DataModel&, rect_t) = nullptr;

    int selected = 0;
    bool displayColumnNames = true;
    bool drawBackground = true;

protected:
    DataModel* model = nullptr;
    std::vector<int> columnDisplayWidths;

    int selectedCol = 0;
    short itemHeight = 20;
    int columnDisplayHeight = 20;

    bool showScrollBar = false;

    ScrollBar sBar;

    Graphics::Font* font;

    void ResetScrollBar();

    class ListEditTextbox : public TextBox {
    public:
        ListEditTextbox(ListView* lv, rect_t bounds) : TextBox(bounds, false) { SetParent(lv); }
    };

    ListEditTextbox editbox = ListEditTextbox(this, {0, 0, 0, 0});
    bool editing = false;
};

class GridItem {
public:
    const Surface* icon = nullptr;
    std::string name;
};

class GridView : public Widget {
public:
    GridView(rect_t bounds) : Widget(bounds) {}
    ~GridView() = default;

    void Paint(surface_t* surface);

    void OnMouseDown(vector2i_t mousePos);
    void OnMouseUp(vector2i_t mousePos);
    void OnMouseMove(vector2i_t mousePos);
    void OnDoubleClick(vector2i_t mousePos);
    void OnKeyPress(int key);

    int AddItem(GridItem& item);

    void ClearItems() {
        items.clear();
        ResetScrollBar();
    }

    void UpdateFixedBounds();

    void (*OnSubmit)(GridItem&, GridView*) = nullptr;
    void (*OnSelect)(GridItem&, GridView*) = nullptr;

    int selected = -1;

protected:
    std::vector<GridItem> items;

    const vector2i_t itemSize = {96, 80};
    int itemsPerRow = 1;
    bool showScrollBar = false;

    ScrollBar sBar;

    void ResetScrollBar();
    int PosToItem(vector2i_t relPos) {
        if (!items.size())
            return -1;

        int column = 0;
        int row = 0;

        if (relPos.x) {
            column = relPos.x / itemSize.x;
            if (column >= itemsPerRow) {
                return -1;
            }
        }

        if (relPos.y) {
            row = relPos.y / itemSize.y;
            if (row > static_cast<int>(items.size()) / itemsPerRow) {
                return -1;
            }
        }

        return row * itemsPerRow + column;
    }
};

class FileView : public Container {
protected:
    vector2i_t pathBoxPadding = {6, 6};
    int pathBoxHeight = 24;
    int sidepanelWidth = 132;
    char** filePointer;

    void (*OnFileOpened)(const char*, FileView*) = nullptr;

    GridView* fileList;
    TextBox* pathBox;

    Widget* active;

    ListColumn nameCol, sizeCol;

public:
    static const Surface* diskIcon;
    static const Surface* folderIcon;
    static const Surface* fileIcon;
    static const Surface* textFileIcon;
    static const Surface* jsonFileIcon;
    static const Surface* ramIcon;
    static const Surface* diskIconSml;
    static const Surface* folderIconSml;

    std::string currentPath;
    FileView(rect_t bounds, const char* path, void (*_OnFileOpened)(const char*, FileView*) = nullptr);

    void Refresh();

    void OnSubmit(std::string& path);
    static void OnListSubmit(GridItem& item, GridView* list);
    static void OnTextBoxSubmit(TextBox* textBox);

    inline int SidepanelWidth() { return sidepanelWidth; }

    void (*OnFileSelected)(std::string& file, FileView* fv) = nullptr;
};

class ScrollView : public Container {
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
} // namespace Lemon::GUI

#pragma once

#include <gfx/surface.h>
#include <gfx/graphics.h>

#include <vector>

namespace Lemon::GUI {
    enum Position {
        Absolute,   // Fixed position relative to parent
        Static,     // Fits flow of container
    };

    enum Size {
        Fixed,      // Fixed Size in Pixels
        Relative,   // Size is a offset from the edge of the container
    };

    class Widget {
    protected:
        Widget* parent = nullptr;

        short layoutPosition = Position::Absolute;
        short layoutSizeX = Size::Fixed;
        short layoutSizeY = Size::Fixed;

        rect_t bounds;
        rect_t fixedBounds;
    public:
        Widget(rect_t bounds);

        virtual void SetParent(Widget* newParent) { parent = newParent };

        virtual void SetLayout(Position newPosition, Size newSizeX, Size newSizeY){ layoutPosition = newPosition; layoutSizeX = newSizeX; layoutSizeY = newSizeY; };

        virtual void Paint(surface_t* surface);

        virtual void OnMouseDown(vector2i_t mousePos);
        virtual void OnMouseUp(vector2i_t mousePos);
        virtual void OnMouseMove(vector2i_t mousePos);
        virtual void OnKeyPress(int key);

        virtual void UpdateFixedBounds();
    };

    class Container : public Widget {
    protected:
        std::vector<Widget*> children;

    public:
        void AddWidget(Widget* w);

        void Paint(surface_t* surface);

        void OnMouseDown(vector2i_t mousePos);
        void OnMouseUp(vector2i_t mousePos);
        void OnMouseMove(vector2i_t mousePos);
        void OnKeyPress(int key);

        void UpdateFixedBounds();
    };
}
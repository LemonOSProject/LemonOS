#pragma once

#include <Lemon/Graphics/Font.h>
#include <Lemon/Graphics/Surface.h>
#include <Lemon/Graphics/Types.h>

#include <string>

#include <assert.h>

namespace Lemon::Graphics {
class TextObject {
public:
    enum {
        RenderLeftToRight,
        RenderNormal = RenderLeftToRight,
        RenderRightToLeft,
        RenderVerticalDownToUp,
        RenderVerticalUpToDown,
    };

public:
    TextObject(vector2i_t pos, const std::string& text, Font* font = DefaultFont());
    TextObject(vector2i_t pos, const char* text, Font* font = DefaultFont());
    TextObject(vector2i_t pos = {0, 0}, Font* font = DefaultFont());

    /////////////////////////////
    /// \brief Render TextObject on surface
    ///
    /// \param surface Surface to render to
    /////////////////////////////
    inline void BlitTo(Surface* dest) { if(m_textDirty) Update(); dest->AlphaBlit(&m_surface, m_pos); }

    /////////////////////////////
    /// \brief Set TextObject font
    ///
    /// \param font Font object
    /////////////////////////////
    inline void SetFont(Font* font) {
        assert(font);

        m_font = font;
        m_textDirty = true;
    }

    /////////////////////////////
    /// \brief Set TextObject text
    ///
    /// \param text Text to render
    /////////////////////////////
    inline void SetText(const char* t) {
        m_text = t;

        m_textDirty = true;
    }

    /////////////////////////////
    /// \brief Set TextObject text
    ///
    /// \param text Text to render
    /////////////////////////////
    inline void SetText(const std::string& t) {
        m_text = t;

        m_textDirty = true;
    }

    /////////////////////////////
    /// \brief Set position of TextObject
    ///
    /// \param pos New position
    /////////////////////////////
    inline void SetPos(const Vector2i& pos) { m_pos = pos; }

    /////////////////////////////
    /// \brief Set colour of TextObject
    ///
    /// \param colour New colour
    /////////////////////////////
    inline void SetColour(const RGBAColour& colour) { m_colour = colour; }

    /////////////////////////////
    /// \brief Get size of the font being rendered
    ///
    /// Note that this is the font size not the screen size of the text object
    ///
    /// \return Font size as int
    /////////////////////////////
    inline int FontSize() {
        assert(m_font);

        return m_font->height;
    }

    /////////////////////////////
    /// \brief Get size of the text object in pixels
    //
    /// Get size of TextObject when rendered on screen
    ///
    /// \return Return Vector2i containing the size of the text object in pixels
    /////////////////////////////
    inline vector2i_t Size() {
        if (m_textDirty) {
            Update();
        }
        return m_textSize;
    }

    /////////////////////////////
    /// \brief Get position of TextObject
    //
    /// Get position in pixels of TextObject
    ///
    /// \return Return Vector2i containing the position of the TextObject on the surface
    /////////////////////////////
    inline const Vector2i& Pos() const { return m_pos; }

protected:
    std::string m_text;
    Font* m_font = nullptr;

    vector2i_t m_pos;

    bool m_textDirty = true;
    vector2i_t m_textSize = {0, 0};

    int m_renderMode = RenderNormal;
    rgba_colour_t m_colour = {0, 0, 0, 255};
    Surface m_surface{};

    void CalculateSizes();
    void Update();
};
} // namespace Lemon::Graphics

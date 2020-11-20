#pragma once

#include <gfx/font.h>
#include <gfx/types.h>
#include <gfx/surface.h>

#include <string>

#include <assert.h>

namespace Lemon::Graphics{
    class TextObject {
    public:
        enum {
            RenderLeftToRight,
            RenderNormal = RenderLeftToRight,
            RenderRightToLeft,
            RenderVerticalDownToUp,
            RenderVerticalUpToDown,
        };
    protected:
        std::string text;
        Font* font = nullptr;

        vector2i_t pos;

        bool textDirty = true;
        vector2i_t textSize;

        int renderMode = RenderNormal;
        
        rgba_colour_t colour = {0, 0, 0, 255};

        void CalculateSizes();
    public:
        TextObject(vector2i_t pos, std::string& text, Font* font = DefaultFont());
        TextObject(vector2i_t pos, const char* text, Font* font = DefaultFont());
        TextObject(vector2i_t pos = {0, 0}, Font* font = DefaultFont());

        /////////////////////////////
        /// \brief Render TextObject on surface
        ///
        /// \param surface Surface to render to
        /////////////////////////////
        void Render(surface_t* surface);

        /////////////////////////////
        /// \brief Set TextObject font
        ///
        /// \param font Font object
        /////////////////////////////
        inline void SetFont(Font* font){
            assert(font);

            this->font = font;
            textDirty = true;
        }

        /////////////////////////////
        /// \brief Set position of TextObject
        ///
        /// \param pos New position
        /////////////////////////////
        inline void SetPos(vector2i_t pos){
            this->pos = pos;
        }

        /////////////////////////////
        /// \brief Set colour of TextObject
        ///
        /// \param colour New colour
        /////////////////////////////
        inline void SetColour(rgba_colour_t colour){
            this->colour = colour;
        }

        /////////////////////////////
        /// \brief Get size of the font being rendered
        ///
        /// Note that this is the font size not the screen size of the text object
        ///
        /// \return Font size as int
        /////////////////////////////
        inline int FontSize(){
            assert(font);

            return font->height;
        }

        /////////////////////////////
        /// \brief Get size of the text object in pixels
        //
        /// Get size of TextObject when rendered on screen
        ///
        /// \return Return Vector2i containing the size of the text object in pixels
        /////////////////////////////
        inline vector2i_t Size(){
            if(textDirty){
                CalculateSizes();
            }
            return textSize;
        }

        /////////////////////////////
        /// \brief Get position of TextObject
        //
        /// Get position in pixels of TextObject
        ///
        /// \return Return Vector2i containing the position of the TextObject on the surface
        /////////////////////////////
        inline vector2i_t Pos(){
            return pos;
        }
    };
}
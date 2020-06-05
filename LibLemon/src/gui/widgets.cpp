#include <gui/widgets.h>

#include <core/keyboard.h>
#include <ctype.h>
#include <string>
#include <math.h>

namespace Lemon::GUI {
    Widget::Widget() {}

    Widget::Widget(rect_t bounds, LayoutSize newSizeX, LayoutSize newSizeY){
        this->bounds = bounds;

        sizeX = newSizeX;
        sizeY = newSizeY;

        UpdateFixedBounds();
    }

    Widget::~Widget(){}

    void Widget::Paint(surface_t* surface){}

    void Widget::OnMouseDown(vector2i_t mousePos){}

    void Widget::OnMouseUp(vector2i_t mousePos){}

    void Widget::OnHover(vector2i_t mousePos){}

    void Widget::OnMouseMove(vector2i_t mousePos) {}

    void Widget::OnKeyPress(int key) {}

    void Widget::UpdateFixedBounds(){
        fixedBounds.pos = bounds.pos;

        if(parent){
            fixedBounds.pos += parent->GetFixedBounds().pos;

            if(sizeX == LayoutSize::Stretch){
                if(align == WidgetAlignment::WAlignRight)
                    fixedBounds.width = (parent->GetFixedBounds().size.x - bounds.pos.x) - parent->GetFixedBounds().pos.x;
                else
                    fixedBounds.width = parent->bounds.width - bounds.width - bounds.x;
            } else {
                fixedBounds.width = bounds.width;
            }
            
            if(sizeY == LayoutSize::Stretch){
                fixedBounds.height = parent->bounds.height - bounds.height - bounds.y;
            } else {
                fixedBounds.height = bounds.height;
            }

            if(align == WidgetAlignment::WAlignRight){
                fixedBounds.pos.x = (parent->GetFixedBounds().pos.x + parent->GetFixedBounds().size.x) - bounds.pos.x - fixedBounds.width;
            }
        } else {
            fixedBounds.size = bounds.size;
        }
    }

    //////////////////////////
    // Contianer
    //////////////////////////
    Container::Container(rect_t bounds) : Widget(bounds) {

    }

    void Container::AddWidget(Widget* w){
        children.push_back(w);

        w->SetParent(this);
    }

    void Container::Paint(surface_t* surface){
        if(background.a == 255) Graphics::DrawRect(fixedBounds, background, surface);

        for(Widget* w : children){
            w->Paint(surface);
        }
    }

    void Container::OnMouseDown(vector2i_t mousePos){
        for(Widget* w : children){
            if(Graphics::PointInRect(w->GetFixedBounds(), mousePos)){
                active = w;
                w->OnMouseDown(mousePos);
                break;
            }
        }
    }

    void Container::OnMouseUp(vector2i_t mousePos){
        for(Widget* w : children){
            if(Graphics::PointInRect(w->GetFixedBounds(), mousePos)){
                w->OnMouseUp(mousePos);
                break;
            }
        }
    }

    void Container::OnMouseMove(vector2i_t mousePos){
        if(active){
            active->OnMouseMove(mousePos);
        }
    }

    void Container::OnKeyPress(int key){
        if(active) {
            active->OnKeyPress(key);
        }
    }

    void Container::UpdateFixedBounds(){
        for(Widget* w : children){
            w->UpdateFixedBounds();
        }

        Widget::UpdateFixedBounds();
    }
    
    //////////////////////////
    // Button
    //////////////////////////
    Button::Button(const char* _label, rect_t _bounds) : Widget(_bounds) {
        strcpy(label, _label);
        labelLength = Graphics::GetTextLength(label);
    }

    void Button::DrawButtonBorders(surface_t* surface, bool white){
        vector2i_t btnPos = fixedBounds.pos;
        rect_t bounds = fixedBounds;

        if(white){
            Graphics::DrawRect(bounds.pos.x+1, btnPos.y, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x, btnPos.y + 1, 1, bounds.size.y - 2, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x + bounds.size.x - 1, btnPos.y + 1, 1, bounds.size.y-2, 250, 250, 250, surface);
        } else {
            Graphics::DrawRect(btnPos.x+1, btnPos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x, btnPos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x + bounds.size.x - 1, btnPos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
        }
    }

    void Button::DrawButtonLabel(surface_t* surface, bool white){
        rgba_colour_t colour;
        vector2i_t btnPos = fixedBounds.pos;

        if(white){
            colour = {250, 250, 250, 255};
        } else {
            colour = {0, 0, 0, 255};
        }

        if(labelAlignment = TextAlignment::Centre){
            Graphics::DrawString(label, btnPos.x + (fixedBounds.size.x / 2) - (labelLength / 2), btnPos.y + (bounds.size.y / 2 - 8), colour.r, colour.g, colour.b, surface);
        } else {
            Graphics::DrawString(label, btnPos.x + 2, btnPos.y + (fixedBounds.height / 2 - 8), colour.r, colour.g, colour.b, surface);
        }
    }

    void Button::Paint(surface_t* surface){
        vector2i_t btnPos = fixedBounds.pos;
        rect_t bounds = fixedBounds;

        if(pressed){
            Graphics::DrawRect(btnPos.x+1, btnPos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224/1.1, 224/1.1, 219/1.1, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);

            DrawButtonBorders(surface, false);
        } else {
            switch(style){
                case 1: // Blue
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{50,50,150,255},{45,45,130,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                case 2: // Red
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{180,60,60,255},{160,55,55,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                case 3: // Yellow
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{200,160,50,255},{180,140,45,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                default:
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{250,250,250,255},{235,235,230,255},surface);
                    DrawButtonBorders(surface, false);
                    if(drawText) DrawButtonLabel(surface, false);
                    break;
            }
            
        }
    }

    void Button::OnMouseDown(vector2i_t mousePos){
        pressed = true;
    }

    void Button::OnMouseUp(vector2i_t mousePos){
        if(OnPress) OnPress(this);

        pressed = false;
    }

    //////////////////////////
    // Bitmap
    //////////////////////////

    Bitmap::Bitmap(rect_t _bounds) : Widget(_bounds) {
        surface.buffer = (uint8_t*)malloc(fixedBounds.size.x*fixedBounds.size.y*4);
        surface.width = fixedBounds.size.x;
        surface.height = fixedBounds.size.y;
    }
    
    Bitmap::Bitmap(rect_t _bounds, surface_t* surf){
        bounds = {bounds.pos, (vector2i_t){surf->width, surf->height}};
        surface = *surf;
    }

    void Bitmap::Paint(surface_t* surface){
        Graphics::surfacecpy(surface, &this->surface, fixedBounds.pos);
    }

    //////////////////////////
    // Label
    //////////////////////////
    Label::Label(const char* _label, rect_t _bounds) : Widget(_bounds) {
        label = _label;
    }

    void Label::Paint(surface_t* surface){
        Graphics::DrawString(label.c_str(), fixedBounds.pos.x, fixedBounds.pos.y, 0, 0, 0, surface);
    }

    //////////////////////////
    // Scroll Bar
    //////////////////////////
    void ScrollBar::ResetScrollBar(int displayHeight, int areaHeight){
        double scrollDisplayRange = ((double)areaHeight) / displayHeight; // Essentially how many 'displayHeight's in areaHeight
        scrollIncrement = ceil(scrollDisplayRange);
        scrollBar.size.y = displayHeight / scrollDisplayRange;
        scrollBar.pos.y = 0;
        scrollPos = 0;
        height = displayHeight;
    }

    void ScrollBar::Paint(surface_t* surface, vector2i_t offset, int width){
        Graphics::DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
        if(pressed) 
            Graphics::DrawRect(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y, 224/1.1, 224/1.1, 219/1.1, surface);
        else 
            Graphics::DrawGradientVertical(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y,{250,250,250,255},{235,235,230,255},surface);
    }

    void ScrollBar::OnMouseDownRelative(vector2i_t mousePos){
        if(mousePos.y > scrollBar.pos.y && mousePos.y < scrollBar.pos.y + scrollBar.size.y){
            pressed = true;
            pressOffset = mousePos.y - scrollBar.pos.y;
        }
    }

    void ScrollBar::OnMouseMoveRelative(vector2i_t relativePosition){
        if(pressed){
            scrollBar.pos.y = relativePosition.y - pressOffset;
            if(scrollBar.pos.y + scrollBar.size.y > height) scrollBar.pos.y = height - scrollBar.size.y;
            if(scrollBar.pos.y < 0) scrollBar.pos.y = 0;
            scrollPos = scrollBar.pos.y * scrollIncrement;
        }
    }

    void ScrollBarHorizontal::ResetScrollBar(int displayWidth, int areaWidth){
        double scrollDisplayRange = ((double)areaWidth) / displayWidth; // Essentially how many 'displayHeight's in areaHeight
        scrollIncrement = ceil(scrollDisplayRange);
        scrollBar.size.x = displayWidth / scrollDisplayRange;
        scrollBar.pos.x = 0;
        scrollPos = 0;
        width = displayWidth;
    }

    void ScrollBarHorizontal::Paint(surface_t* surface, vector2i_t offset, int height){
        Graphics::DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
        if(pressed) 
            Graphics::DrawRect(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height, 224/1.1, 224/1.1, 219/1.1, surface);
        else 
            Graphics::DrawGradient(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height,{250,250,250,255},{235,235,230,255},surface);
    }

    void ScrollBarHorizontal::OnMouseDownRelative(vector2i_t mousePos){
        if(mousePos.x > scrollBar.pos.x && mousePos.x < scrollBar.pos.x + scrollBar.size.x){
            pressed = true;
            pressOffset = mousePos.x - scrollBar.pos.x;
        }
    }

    void ScrollBarHorizontal::OnMouseMoveRelative(vector2i_t relativePosition){
        if(pressed){
            scrollBar.pos.x = relativePosition.x - pressOffset;
            if(scrollBar.pos.x + scrollBar.size.x > width) scrollBar.pos.x = width - scrollBar.size.x;
            if(scrollBar.pos.x < 0) scrollBar.pos.x = 0;
            scrollPos = scrollBar.pos.x * scrollIncrement;
        }
    }

    //////////////////////////
    // Text Box
    //////////////////////////
    TextBox::TextBox(rect_t bounds, bool multiline) : Widget(bounds) {
        this->multiline = multiline;
        font = Graphics::GetFont("default");
        contents.push_back(std::string());
    }

    void TextBox::Paint(surface_t* surface){
        Graphics::DrawRect(fixedBounds.pos.x, fixedBounds.pos.y, fixedBounds.size.x, fixedBounds.size.y, 255, 255, 255, surface);
        int xpos = 2;
        int ypos = 2;
        for(size_t i = 0; i < contents.size(); i++){
            for(size_t j = 0; j < contents[i].length(); j++){
                if(contents[i][j] == '\t'){
                    xpos += font->tabWidth * font->width;
                    continue;
                } else if (isspace(contents[i][j])) {
                    xpos += font->width;
                    continue;
                }
                else if (!isgraph(contents[i][j])) continue;

                xpos += Graphics::DrawChar(contents[i][j], fixedBounds.pos.x + xpos, fixedBounds.pos.y + ypos - sBar.scrollPos, textColour.r, textColour.g, textColour.b, surface, font);

                if((xpos > (fixedBounds.size.x - 8 - 16))){
                    //xpos = 0;
                    //ypos += font->height + lineSpacing;
                    break;
                }
            }
            ypos += font->height + lineSpacing;
            xpos = 0;
            if(ypos - sBar.scrollPos + font->height + lineSpacing >= fixedBounds.size.y) break;
        }

        timespec t;
        clock_gettime(CLOCK_BOOTTIME, &t);

        long msec = (t.tv_nsec / 1000000.0);
        if(msec < 250 || (msec > 500 && msec < 750)) // Only draw the cursor for a quarter of a second so it blinks
            Graphics::DrawRect(fixedBounds.pos.x + Graphics::GetTextLength(contents[cursorPos.y].c_str(), cursorPos.x, font) + 2, fixedBounds.pos.y + cursorPos.y * (font->height + lineSpacing) - 1 - sBar.scrollPos + 2, 2, font->height + 2, 0, 0, 0, surface);

        if(multiline)
            sBar.Paint(surface, {fixedBounds.pos.x + fixedBounds.size.x - 16, fixedBounds.pos.y});
    }

    void TextBox::LoadText(char* text){
        char* text2 = text;
        int lineCount = 1;

        if(multiline){
            while(*text2){
                if(*text2 == '\n') lineCount++;
                text2++;
            }

            text2 = text;
            for(int i = 0; i < lineCount; i++){
                char* end = strchr(text2, '\n');
                if(end){
                    int len = (uintptr_t)(end - text2);
                    if(len > strlen(text2)) break;

                    contents.push_back(std::string(text2, len));

                    text2 = end + 1;
                } else {
                    contents.push_back(std::string(text2));
                }
            }
            
            this->lineCount = contents.size();
            ResetScrollBar();
        } else {
            contents.push_back(std::string(text2));
        }
    }

    void TextBox::OnMouseDown(vector2i_t mousePos){
        mousePos.x -= fixedBounds.pos.x;
        mousePos.y -= fixedBounds.pos.y;

        if(multiline && mousePos.x > fixedBounds.size.x - 16){
            sBar.OnMouseDownRelative({mousePos.x + fixedBounds.size.x - 16, mousePos.y});
            return;
        }

        cursorPos.y = (sBar.scrollPos + mousePos.y) / (font->height + lineSpacing);
        if(cursorPos.y >= contents.size()) cursorPos.y = contents.size() - 1;

        int dist = 0;
        for(cursorPos.x = 0; cursorPos.x < contents[cursorPos.y].length(); cursorPos.x++){
            dist += Graphics::GetCharWidth(contents[cursorPos.y][cursorPos.x], font);
            if(dist >= mousePos.x){
                break;
            }
        }
    }

    void TextBox::OnMouseMove(vector2i_t mousePos){
        if(multiline && sBar.pressed){
            sBar.OnMouseMoveRelative({0, mousePos.y - fixedBounds.pos.y});
        }
    }

    void TextBox::OnMouseUp(vector2i_t mousePos){
        sBar.pressed = false;
    }


    void TextBox::ResetScrollBar(){
        sBar.ResetScrollBar(fixedBounds.size.y, contents.size() * (font->height + lineSpacing));
    }

    void TextBox::OnKeyPress(int key){
        if(isprint(key)){
            contents[cursorPos.y].insert(cursorPos.x++, 1, key);
        } else if(key == '\b' || key == KEY_DELETE){
            if(key == KEY_DELETE){ // Delete is essentially backspace but on the character in front so just increment the cursor pos.
                cursorPos.x++;
                if(cursorPos.x > contents[cursorPos.x].length()){
                    if(cursorPos.y < contents.size()){
                        cursorPos.x = contents[++cursorPos.y].length();
                    } else cursorPos.x = contents[cursorPos.y].length();
                }
            }

            if(cursorPos.x) {
                contents[cursorPos.y].erase(--cursorPos.x, 1);
            } else if(cursorPos.y) { // Delete line if not at start of file
                cursorPos.x = contents[cursorPos.y - 1].length(); // Move cursor horizontally to end of previous line
                contents[cursorPos.y - 1] += contents[cursorPos.y]; // Append contents of current line to previous
                contents.erase(contents.begin() + cursorPos.y--); // Remove line and move to previous line

                ResetScrollBar();
            }
        } else if(key == '\n'){
            std::string s = contents[cursorPos.y].substr(cursorPos.x); // Grab the contents of the line after the cursor
            contents[cursorPos.y].erase(cursorPos.x); // Erase all that was after the cursor
            contents.insert(contents.begin() + (++cursorPos.y), s); // Insert new line after cursor and move to that line
            cursorPos.x = 0;
            ResetScrollBar();
        } else if (key == KEY_ARROW_LEFT){ // Move cursor left
            cursorPos.x--;
            if(cursorPos.x < 0){
                if(cursorPos.y){
                    cursorPos.x = contents[--cursorPos.y].length();
                } else cursorPos.x = 0;
            }
        } else if (key == KEY_ARROW_RIGHT){ // Move cursor right
            cursorPos.x++;
            if(cursorPos.x > contents[cursorPos.y].length()){
                if(cursorPos.y < contents.size()){
                    cursorPos.x = contents[++cursorPos.y].length();
                } else cursorPos.x = contents[cursorPos.y].length();
            }
        } else if (key == KEY_ARROW_UP){ // Move cursor up
            if(cursorPos.y){
                cursorPos.y--;
                if(cursorPos.x > contents[cursorPos.y].length()){
                    cursorPos.x = contents[cursorPos.y].length();
                }
            } else cursorPos.x = 0;
        } else if (key == KEY_ARROW_DOWN){ // Move cursor down
            if(cursorPos.y < contents.size()){
                cursorPos.y++;
                if(cursorPos.x > contents[cursorPos.y].length()){
                    cursorPos.x = contents[cursorPos.y].length();
                }
            } else cursorPos.x = contents[cursorPos.y].length();;
        }
    }
}
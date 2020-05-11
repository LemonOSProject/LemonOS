#include <gfx/window/widgets.h>
#include <gfx/window/messagebox.h>
#include <lemon/keyboard.h>

#include <string.h>
#include <stdlib.h>

#include <math.h>
#include <ctype.h>
#include <time.h>

#define BUTTON_COLOUR_OUTLINE_DEFAULT

namespace Lemon::GUI{
    void Widget::Paint(surface_t* surface){}

    void Widget::OnMouseDown(vector2i_t mousePos){}

    void Widget::OnMouseUp(vector2i_t mousePos){}

    void Widget::OnHover(vector2i_t mousePos){}

    void Widget::OnMouseMove(vector2i_t mousePos) {}

    void Widget::OnKeyPress(int key) {}

    Widget::~Widget(){}

    //////////////////////////
    // Text Box
    //////////////////////////
    TextBox::TextBox(rect_t bounds){
        this->bounds = bounds;
        font = Graphics::GetFont("default");
    }

    void TextBox::Paint(surface_t* surface){
        Graphics::DrawRect(bounds.pos.x, bounds.pos.y, bounds.size.x, bounds.size.y, 255, 255, 255, surface);
        int xpos = 0;
        int ypos = 0;
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

                xpos += Graphics::DrawChar(contents[i][j], bounds.pos.x + xpos, bounds.pos.y + ypos - sBar.scrollPos, textColour.r, textColour.g, textColour.b, surface, font);

                if((xpos > (bounds.size.x - 8 - 16))){
                    //xpos = 0;
                    //ypos += font->height + lineSpacing;
                    break;
                }
            }
            ypos += font->height + lineSpacing;
            xpos = 0;
            if(ypos - sBar.scrollPos + font->height + lineSpacing >= bounds.size.y) break;
        }

        timespec t;
        clock_gettime(CLOCK_BOOTTIME, &t);

        long msec = (t.tv_nsec / 1000000.0);
        if(msec < 250 || (msec > 500 && msec < 750)) // Only draw the cursor for a quarter of a second so it blinks
            Graphics::DrawRect(bounds.pos.x + Graphics::GetTextLength(contents[cursorPos.y].c_str(), cursorPos.x, font), bounds.pos.y + cursorPos.y * (font->height + lineSpacing) - 1 - sBar.scrollPos, 2, font->height + 2, 0, 0, 0, surface);

        sBar.Paint(surface, {bounds.pos.x + bounds.size.x - 16, bounds.pos.y});
    }

    void TextBox::LoadText(char* text){
        char* text2 = text;
        int lineCount = 1;
        int lineNum = 0;

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
                lineNum++;
            } else {
                contents.push_back(std::string(text2));
                lineNum++;
            }
        }
        
        this->lineCount = contents.size();
        ResetScrollBar();
    }

    void TextBox::OnMouseDown(vector2i_t mousePos){
        mousePos.x -= bounds.pos.x;
        mousePos.y -= bounds.pos.y;

        if(mousePos.x > bounds.size.x - 16){
            sBar.OnMouseDownRelative({mousePos.x + bounds.size.x - 16, mousePos.y});
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
        if(sBar.pressed){
            sBar.OnMouseMoveRelative({0, mousePos.y - bounds.pos.y});
        }
    }

    void TextBox::OnMouseUp(vector2i_t mousePos){
        sBar.pressed = false;
    }


    void TextBox::ResetScrollBar(){
        sBar.ResetScrollBar(bounds.size.y, contents.size() * (font->height + lineSpacing));
    }

    void TextBox::OnKeyPress(int key){
        if(isprint(key)){
            contents[cursorPos.y].insert(cursorPos.x++, 1, key);
        } else if(key == '\b'){
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
            if(cursorPos.x > contents[cursorPos.x].length()){
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

    //////////////////////////
    // Button
    //////////////////////////
    Button::Button(const char* _label, rect_t _bounds){
        labelLength = Graphics::GetTextLength(label);
        strcpy(label, _label);

        bounds = _bounds;
    }

    void Button::DrawButtonBorders(surface_t* surface, bool white){
        if(white){
            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 250, 250, 250, surface);
            Graphics::DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 250, 250, 250, surface);
        } else {
            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
            Graphics::DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
        }
    }

    void Button::Paint(surface_t* surface){

        if(pressed){
            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224/1.1, 224/1.1, 219/1.1, surface);
            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);

            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
            Graphics::DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
        } else {
            switch(style){
                case 1: // Blue
                    Graphics::DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{50,50,150,255},{45,45,130,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) Graphics::DrawString(label, bounds.pos.x + bounds.size.x / 2 - labelLength/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                    break;
                case 2: // Red
                    Graphics::DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{180,60,60,255},{160,55,55,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) Graphics::DrawString(label, bounds.pos.x + bounds.size.x / 2 - labelLength/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                    break;
                case 3: // Yellow
                    Graphics::DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{200,160,50,255},{180,140,45,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) Graphics::DrawString(label, bounds.pos.x + bounds.size.x / 2 - labelLength/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                    break;
                default:
                    Graphics::DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{250,250,250,255},{235,235,230,255},surface);
                    DrawButtonBorders(surface, false);
                    if(drawText) Graphics::DrawString(label, bounds.pos.x + bounds.size.x / 2 - labelLength/2, bounds.pos.y + bounds.size.y / 2 - 8, 0, 0, 0, surface);
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
    Bitmap::Bitmap(){}

    Bitmap::Bitmap(rect_t _bounds){
        bounds = _bounds;
        surface.buffer = (uint8_t*)malloc(bounds.size.x*bounds.size.y*4);
        surface.width = bounds.size.x;
        surface.height = bounds.size.y;
    }
    
    Bitmap::Bitmap(rect_t _bounds, surface_t* surf){
        bounds = {bounds.pos, (vector2i_t){surf->width, surf->height}};
        surface = *surf;
    }

    void Bitmap::Paint(surface_t* surface){
        Graphics::surfacecpy(surface, &this->surface, bounds.pos);
    }

    //////////////////////////
    // Label
    //////////////////////////
    Label::Label(const char* _label, rect_t _bounds){
        label = _label;

        bounds = _bounds;
    }

    void Label::Paint(surface_t* surface){
        Graphics::DrawString(label.c_str(), bounds.pos.x, bounds.pos.y, 0, 0, 0, surface);
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
    // Scroll View
    //////////////////////////
    ScrollView::ScrollView(rect_t _bounds){
        bounds = _bounds;
    }

    void ScrollView::ResetScrollBar(){
        sBarVert.ResetScrollBar(bounds.size.y - 16, limits.y);
        sBarHor.ResetScrollBar(bounds.size.x - 16, limits.x);
    }

    void ScrollView::AddWidget(Widget* w){
        children.add_back(w);

        vector2i_t wLimits = w->bounds.pos + w->bounds.size;
        if(wLimits.x > limits.x) limits.x = wLimits.x;
        if(wLimits.y > limits.y) limits.y = wLimits.y;

        ResetScrollBar();
    }

    void ScrollView::Paint(surface_t* surface){
        for(int i = 0; i < children.get_length(); i++){
            rect_t _bounds = children[i]->bounds;
            children[i]->bounds = {_bounds.pos + bounds.pos - (vector2i_t){sBarHor.scrollPos, sBarVert.scrollPos}, _bounds.size};

            children[i]->Paint(surface);
            children[i]->bounds = _bounds;
        }

        sBarVert.Paint(surface, {bounds.pos + (vector2i_t){bounds.size.x - 16, 0}});
        sBarHor.Paint(surface, {bounds.pos + (vector2i_t){0, bounds.size.y - 16}});
    }

    void ScrollView::OnMouseDown(vector2i_t mousePos){
        activeWidget = -1;

        mousePos.x -= bounds.pos.x;
        mousePos.y -= bounds.pos.y;

        if(mousePos.y > bounds.size.y - 16){
            sBarHor.OnMouseDownRelative({mousePos.y - bounds.size.y + 16, mousePos.x});
            return;
        }
        
        if(mousePos.x > bounds.size.x - 16){
            sBarVert.OnMouseDownRelative({mousePos.y, mousePos.x - bounds.size.x + 16});
            return;
        }

        for(int i = 0; i < children.get_length(); i++){
            Widget* child = children[i];
            if(Graphics::PointInRect(child->bounds, mousePos)){
                child->OnMouseDown(mousePos);
                activeWidget = i;
                break;
            }
        }
    }

    void ScrollView::OnMouseUp(vector2i_t mousePos){
        mousePos.x -= bounds.pos.x;
        mousePos.y -= bounds.pos.y;

        sBarVert.pressed = false;
        sBarHor.pressed = false;

		if(lastPressedWidget >= 0){
			children[lastPressedWidget]->OnMouseUp(mousePos);
		}
		lastPressedWidget = -1;
    }

    void ScrollView::OnMouseMove(vector2i_t mousePos){
        mousePos.x -= bounds.pos.x;
        mousePos.y -= bounds.pos.y;
            
        if(sBarVert.pressed){
            sBarVert.OnMouseMoveRelative(mousePos);
            return;
        } else if(sBarHor.pressed){
            sBarHor.OnMouseMoveRelative(mousePos);
            return;
        }

		if(lastPressedWidget >= 0){
			children[lastPressedWidget]->OnMouseMove(mousePos);
		}
    }

    //////////////////////////
    // ListView
    //////////////////////////
    ListItem::ListItem(char* _text){
        text = (char*)malloc(strlen(_text) + 1);//new char[strlen(_text) + 1];
        strcpy(text, _text);
    }

    ListView::ListView(rect_t _bounds, int itemHeight){
        bounds = _bounds;
        iBounds = bounds;
        this->itemHeight = itemHeight;
    }

    ListView::~ListView(){
    }

    void ListView::ResetScrollBar(){
        double iCount = contents.get_length();

        sBar.ResetScrollBar(iBounds.size.y, iCount * itemHeight);
    }

    void ListView::Paint(surface_t* surface){
        for(int i = 0; i < contents.get_length(); i++){
            ListItem* item = contents.get_at(i);

            if((i * itemHeight - sBar.scrollPos) < 0) continue;

            if(selected == i){
                Graphics::DrawRect(iBounds.pos.x + 2,iBounds.pos.y + i * itemHeight + 2 - sBar.scrollPos, iBounds.size.x - 24, itemHeight - 4, {165,/*24*/32,24}, surface);
            }
            Graphics::DrawString(item->text, iBounds.pos.x + 4, iBounds.pos.y + i * itemHeight + (itemHeight / 2) - 6 - sBar.scrollPos, 0, 0, 0, surface);
        }

        sBar.Paint(surface, {iBounds.pos.x + iBounds.size.x - 16, iBounds.pos.y});
    }

    void ListView::OnMouseDown(vector2i_t mousePos){
        if(mousePos.x > iBounds.pos.x + iBounds.size.x - 16){
            sBar.OnMouseDownRelative({mousePos.x - iBounds.pos.x + iBounds.size.x - 16, mousePos.y - iBounds.pos.y});
            return;
        }
        selected = floor(((double)mousePos.y + sBar.scrollPos - iBounds.pos.y) / itemHeight);

        if(selected >= contents.get_length()) selected = contents.get_length();
        if(selected < 0) selected = 0;
    }

    void ListView::OnMouseMove(vector2i_t mousePos){
        if(sBar.pressed){
            sBar.OnMouseMoveRelative({0, mousePos.y - iBounds.pos.y});
        }
    }

    void ListView::OnMouseUp(vector2i_t mousePos){
        sBar.pressed = false;
    }

    void ListView::OnKeyPress(int key){
        if(key == KEY_ARROW_DOWN) selected++;
        else if (key == KEY_ARROW_UP) selected--;
        else return;

        if(selected >= contents.get_length()) selected = contents.get_length();
        if(selected < 0) selected = 0;

        // Make sure the selection is within view
        if((selected * itemHeight - sBar.scrollPos) < 0) sBar.scrollPos = selected * itemHeight;
        else if(((selected + 1) * itemHeight - sBar.scrollPos) > iBounds.size.y) sBar.scrollPos = selected * itemHeight - iBounds.size.y + itemHeight; // Add 1 to selected to check if bottom is visible
    }

    void ListItem::OnMouseDown(vector2i_t mousePos){
    }

    void ListItem::OnMouseUp(vector2i_t mousePos){
        
    }
}
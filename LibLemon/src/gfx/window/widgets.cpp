#include <gfx/window/widgets.h>

#include <string.h>
#include <stdlib.h>

#include <math.h>
#include <ctype.h>

#define BUTTON_COLOUR_OUTLINE_DEFAULT

void Widget::Paint(surface_t* surface){
}

void Widget::OnMouseDown(vector2i_t mousePos){
}

void Widget::OnMouseUp(vector2i_t mousePos){
}

void Widget::OnHover(vector2i_t mousePos){
}

void Widget::OnMouseMove(vector2i_t mousePos) {}

Widget::~Widget(){}

//////////////////////////
// Text Box
//////////////////////////
TextBox::TextBox(rect_t bounds){
    this->bounds = bounds;
}

void TextBox::Paint(surface_t* surface){
    DrawRect(bounds.pos.x, bounds.pos.y, bounds.size.x, bounds.size.y, 255, 255, 255, surface);
    int xpos = 0;
    int ypos = 0;
    for(size_t i = 0; i < lineCount; i++){
        for(size_t j = 0; j < strlen(contents[i]); j++){
            if(contents[i][j] == '\t'){
                xpos += 8 * 8;
                continue;
            } else if (isspace(contents[i][j])) {
                xpos += 8;
                continue;
            }
            else if (!isgraph(contents[i][j])) continue;

            xpos += DrawChar(contents[i][j], bounds.pos.x + xpos, bounds.pos.y + ypos - sBar.scrollPos, textColour.r, textColour.g, textColour.b, surface);

            if((xpos > (bounds.size.x - 8 - 16))){
                xpos = 0;
                ypos += 13;
            }
        }
        ypos += 13;
        xpos = 0;
        if(ypos - sBar.scrollPos + 12 >= bounds.size.y) break;
    }

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

    contents = (char**)malloc(sizeof(char*) * lineCount);

    text2 = text;
    for(int i = 0; i < lineCount; i++){
        char* end = strchr(text2, '\n');
        if(end){
            int len = (uintptr_t)(end - text2);
            if(len > strlen(text2)) break;

            contents[lineNum] = (char*)malloc(len + 1);
            strncpy(contents[lineNum], text2, len);
            contents[lineNum][len] = 0;

            text2 = end + 1;
            lineNum++;
        } else {
            contents[lineNum] = (char*)malloc(strlen(text2) + 1);
            strcpy(contents[lineNum], text2);
            lineNum++;
        }
    }
    
    this->lineCount = lineCount;
    ResetScrollBar();
}

void TextBox::OnMouseDown(vector2i_t mousePos){
    if(mousePos.x > bounds.pos.x + bounds.size.x - 16){
        sBar.OnMouseDownRelative({mousePos.x - bounds.pos.x + bounds.size.x - 16, mousePos.y - bounds.pos.y});
        return;
    }
    active = true;
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
    sBar.ResetScrollBar(bounds.size.y, lineCount * 12);
}
//////////////////////////
// Button
//////////////////////////
Button::Button(const char* _label, rect_t _bounds){
    labelLength = strlen(_label);
    strcpy(label, _label);

    bounds = _bounds;
}

void Button::DrawButtonBorders(surface_t* surface, bool white){
    if(white){
        DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 250, 250, 250, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 250, 250, 250, surface);
        DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 250, 250, 250, surface);
        DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 250, 250, 250, surface);
    } else {
        DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
        DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
    }
}

void Button::Paint(surface_t* surface){

    if(pressed){
        DrawRect(bounds.pos.x+1, bounds.pos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224/1.1, 224/1.1, 219/1.1, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);

        DrawRect(bounds.pos.x+1, bounds.pos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x+1, bounds.pos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
        DrawRect(bounds.pos.x, bounds.pos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
        DrawRect(bounds.pos.x + bounds.size.x - 1, bounds.pos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
    } else {
        switch(style){
            case 1: // Blue
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{50,50,150,255},{45,45,130,255},surface);
                DrawButtonBorders(surface, true);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                break;
            case 2: // Red
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{180,60,60,255},{160,55,55,255},surface);
                DrawButtonBorders(surface, true);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                break;
            case 3: // Yellow
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{200,160,50,255},{180,140,45,255},surface);
                DrawButtonBorders(surface, true);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 250, 250, 250, surface);
                break;
            default:
                DrawGradientVertical(bounds.pos.x + 1, bounds.pos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{250,250,250,255},{235,235,230,255},surface);
                DrawButtonBorders(surface, false);
                DrawString(label, bounds.pos.x + bounds.size.x / 2 - (labelLength*8)/2, bounds.pos.y + bounds.size.y / 2 - 8, 0, 0, 0, surface);
                break;
        }
        
    }
}

void Button::OnMouseDown(vector2i_t mousePos){
    pressed = true;
}

void Button::OnMouseUp(vector2i_t mousePos){
    if(OnPress) OnPress();

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

void Bitmap::Paint(surface_t* surface){
    for(int i = 0; i < this->surface.height; i++){
        memcpy(surface->buffer + (i + bounds.pos.y) * surface->width * 4 + bounds.pos.x * 4, this->surface.buffer + bounds.size.x * i * 4, bounds.size.x * 4);
    }
}

//////////////////////////
// Label
//////////////////////////
Label::Label(const char* _label, rect_t _bounds){
    labelLength = strlen(_label) + 1;
    label = (char*)malloc(labelLength);
    strcpy(label, _label);

    bounds = _bounds;
}

void Label::Paint(surface_t* surface){
    DrawString(label, bounds.pos.x, bounds.pos.y, 0, 0, 0, surface);
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
    DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
    if(pressed) 
        DrawRect(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y, 224/1.1, 224/1.1, 219/1.1, surface);
    else 
        DrawGradientVertical(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y,{250,250,250,255},{235,235,230,255},surface);
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
    DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
    if(pressed) 
        DrawRect(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height, 224/1.1, 224/1.1, 219/1.1, surface);
    else 
        DrawGradient(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height,{250,250,250,255},{235,235,230,255},surface);
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

void ScrollView::Paint(surface_t* surface){
    for(int i = 0; i < contents.get_length(); i++){
        rect_t _bounds = contents[i]->bounds;
        contents[i]->bounds = {_bounds.pos + _bounds.pos, _bounds.size};

        contents[i]->Paint(surface);
        contents[i]->bounds = _bounds;
    }

    DrawRect(bounds.pos.x + bounds.size.x - 24, bounds.pos.y, 24, bounds.size.y, {120, 120, 110, 255}, surface);
}

void ScrollView::OnMouseDown(vector2i_t mousePos){

}

void ScrollView::OnMouseUp(vector2i_t mousePos){
    
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
            DrawRect(iBounds.pos.x + 2,iBounds.pos.y + i * itemHeight + 2 - sBar.scrollPos, iBounds.size.x - 24, itemHeight - 4, {165,/*24*/32,24}, surface);
        }
        DrawString(item->text, iBounds.pos.x + 4, iBounds.pos.y + i * itemHeight + (itemHeight / 2) - 6 - sBar.scrollPos, 0, 0, 0, surface);
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
}

void ListView::OnMouseMove(vector2i_t mousePos){
    if(sBar.pressed){
        sBar.OnMouseMoveRelative({0, mousePos.y - iBounds.pos.y});
    }
}

void ListView::OnMouseUp(vector2i_t mousePos){
    sBar.pressed = false;
}

void ListItem::OnMouseDown(vector2i_t mousePos){
}

void ListItem::OnMouseUp(vector2i_t mousePos){
    
}

//////////////////////////
// FileView
//////////////////////////

#include <lemon/filesystem.h>
#include <gfx/window/messagebox.h>
#include <unistd.h>
#include <fcntl.h>

FileView::FileView(rect_t _bounds, char* path, char** _filePointer, void(*_fileOpened)(char*, char**)) : ListView(_bounds){
    bounds = _bounds;
    filePointer = _filePointer;
    OnFileOpened = _fileOpened;

    iBounds.pos.y += pathBoxHeight;
    iBounds.size.y -= pathBoxHeight;
    
    currentPath = (char*)malloc(512);
	strcpy(currentPath, path);

	currentDir = open(currentPath, 666);
	int i = 0;
	lemon_dirent_t dirent;
	while(lemon_readdir(currentDir, i++, &dirent)){
		contents.add_back(new ListItem(dirent.name));
	}

    ResetScrollBar();
}

FileView::~FileView(){
}

void FileView::Refresh(){
    close(currentDir);

    currentDir = lemon_open(currentPath, 666);

    if(!currentDir){
        MessageBox("Failed to open directory!", MESSAGEBOX_OK);
    }
    
    for(int i = contents.get_length() - 1; i >= 0; i--){
        delete contents.get_at(i);
    }
    contents.clear();
    
    int i = 0;
    lemon_dirent_t dirent;
    while(lemon_readdir(currentDir, i++, &dirent)){
        contents.add_back(new ListItem(dirent.name));
    }
    
    ResetScrollBar();
}

void FileView::Paint(surface_t* surface){
    DrawString(currentPath, bounds.pos.x + 4, bounds.pos.y + 4, 0, 0, 0, surface);

    ListView::Paint(surface);
}

void FileView::OnMouseDown(vector2i_t mousePos){
    if(mousePos.y - bounds.pos.y > pathBoxHeight){
        ListView::OnMouseDown(mousePos);
    }
}

void FileView::OnMouseUp(vector2i_t mousePos){
    if(mousePos.y - bounds.pos.y > pathBoxHeight){
        ListView::OnMouseUp(mousePos);
    }
}

void FileView::OnSubmit(){
    ListItem* item = contents.get_at(selected);
    lemon_dirent_t dirent;
    lemon_readdir(currentDir, selected, &dirent);
    if(dirent.type & FS_NODE_DIRECTORY){
        close(currentDir);

        strcpy(currentPath + strlen(currentPath), dirent.name);
        strcpy(currentPath + strlen(currentPath), "/");

        currentDir = lemon_open(currentPath, 666);

        if(!currentDir){
            MessageBox("Failed to open directory!", MESSAGEBOX_OK);
        }
        
        for(int i = contents.get_length() - 1; i >= 0; i--){
            delete contents.get_at(i);
        }
        contents.clear();
        
        int i = 0;
        while(lemon_readdir(currentDir, i++, &dirent)){
            contents.add_back(new ListItem(dirent.name));
        }

        ResetScrollBar();
    } else if(OnFileOpened) {
        char* str = (char*)malloc(strlen(currentPath) + strlen(dirent.name) + 1 /*Separator*/ + 1 /*Null Terminator*/);

        strcpy(str, currentPath);
        strcpy(str + strlen(str), "/");
        strcpy(str + strlen(str), dirent.name);

        OnFileOpened(str, filePointer);

        free(str);
    }
}
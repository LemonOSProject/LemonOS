#include <videoconsole.h>

#include <liballoc.h>
#include <math.h>
#include <string.h>
#include <video.h>

VideoConsole::VideoConsole(int x, int y, int width, int height){
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;

    cursorX = 0;
    cursorY = 0;

    widthInCharacters = floor(width / 8);
    heightInCharacters = floor(height / 8);

    characterBuffer = (ConsoleCharacter*)kmalloc(widthInCharacters*heightInCharacters*4); // One ConsoleCharacter is 4 bytes (char, r, g, b)
    memset(characterBuffer, 0, widthInCharacters*heightInCharacters*4);

}

void VideoConsole::Update(){
    video_mode_t videoMode = Video::GetVideoMode();
    Video::DrawRect(0,0,videoMode.width, videoMode.height, 32, 32, 32);
    for(int i = 0; i < heightInCharacters; i++){
        for(int j = 0; j < widthInCharacters; j++){
            ConsoleCharacter c = characterBuffer[i*widthInCharacters + j];
            if(c.c == 0) continue;
            Video::DrawChar(c.c, x + j * 8, y + i * 8, c.r, c.g, c.b);
        }
    }
}

void VideoConsole::Print(char c, uint8_t r, uint8_t g, uint8_t b){
    switch(c){
        case '\n':
            cursorX = 0;
            cursorY++;
            break;
        case '\b':
            cursorX--;
            if(cursorX < 0){
                cursorX = widthInCharacters - 1;
                cursorY--;
                if(cursorY < 0){
                    cursorX = cursorY = 0;
                }
            }
            break;
        default:
            characterBuffer[cursorY * widthInCharacters + cursorX] = {c, r, g, b};
            cursorX++;
            break;
    }
    if(cursorX >= widthInCharacters){
        cursorX = 0;
        cursorY++;
    }
    if(cursorY >= heightInCharacters) Scroll();
}

void VideoConsole::Clear(uint8_t r, uint8_t g, uint8_t b){
    memset(characterBuffer,0, widthInCharacters*heightInCharacters*sizeof(ConsoleCharacter));
    cursorX = 0;
    cursorY = 0;
}

void VideoConsole::Print(char* str, uint8_t r, uint8_t g, uint8_t b){
    while (*str != '\0'){
		Print(*str++, r, g, b);
	}
}

void VideoConsole::Scroll(){
    memcpy(characterBuffer,(void*)(characterBuffer + widthInCharacters*sizeof(ConsoleCharacter)), widthInCharacters*(heightInCharacters-1)*sizeof(ConsoleCharacter));
}
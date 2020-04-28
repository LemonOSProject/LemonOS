#include "paint.h"

#include <gfx/window/window.h>
#include <gfx/window/messagebox.h>
#include <stdio.h>
#include <stdlib.h>
#include <lemon/ipc.h>
#include <gfx/window/filedialog.h>

#define BRUSH_CALLBACK(x) void OnPressBrush ## x () { canvas->currentBrush = brushes[x]; }
#define SCALE_CALLBACK(x) void OnPressBrushScale ## x () { canvas->brushScale = 0.01 * x; }

win_info_t winInfo{
    .x = 0,
    .y  = 0,
    .width = 736,
    .height = 496,
    .flags = 0,
};

Window* window;
Canvas* canvas;
List<Brush*> brushes;

void Colour::Paint(surface_t* surface){
    DrawRect(bounds, colour, surface);
}

void Colour::OnMouseDown(vector2i_t mousePos){
    canvas->colour = colour;
}

uint8_t brush0[] = {
    255
};

uint8_t brush1[] = {
    0,80,156,190,190,156,80,0,
    80,190,255,255,255,255,190,80,
    156,255,255,255,255,255,255,156,
    190,255,255,255,255,255,255,190,
    190,255,255,255,255,255,255,190,
    156,255,255,255,255,255,255,156,
    80,190,255,255,255,255,190,80,
    0,80,156,190,190,156,80,0,
};

BRUSH_CALLBACK(0)
BRUSH_CALLBACK(1)

SCALE_CALLBACK(25)
SCALE_CALLBACK(50)
SCALE_CALLBACK(100)
SCALE_CALLBACK(150)
SCALE_CALLBACK(200)

#define DEFAULT_COLOUR_COUNT 16
rgba_colour_t defaultColours[DEFAULT_COLOUR_COUNT]{
    {0, 0, 0, 255},
    {255, 255, 255, 255},
    {255, 0, 0, 255},
    {0, 255, 0, 255},
    {0, 0, 255, 255},
    {255, 255, 0, 255},
    {255, 0, 255, 255},
    {0, 255, 255, 255},
    {64, 64, 64, 255},
    {128, 128, 128, 255},
    {128, 0, 0, 255},
    {0, 128, 0, 255},
    {0, 0, 128, 255},
    {128, 128, 0, 255},
    {128, 0, 128, 255},
    {0, 128, 128, 255},
};

void LoadImage(char* path){
    if(!path){
        MessageBox("Invalid Filepath", MESSAGEBOX_OK);
        return;
    }

    FILE* image = fopen(path, "r");

    if(!image){
        MessageBox("Failed to open image!", MESSAGEBOX_OK);
        return;
    }

    fseek(image, 0, SEEK_END);
    size_t imageSize = ftell(image);
    fseek(image, 0, SEEK_SET);

    uint8_t* imageBuffer = (uint8_t*)malloc(imageSize);
    fread(imageBuffer, imageSize, 1, image);

    bitmap_info_header_t* infoHeader = ((bitmap_info_header_t*)(imageBuffer + sizeof(bitmap_file_header_t)));

    if(canvas->surface.buffer){
        free(canvas->surface.buffer);
        canvas->surface.buffer = (uint8_t*)malloc(infoHeader->width * infoHeader->height * 4);
    } else
        canvas->surface.buffer = (uint8_t*)malloc(infoHeader->width * infoHeader->height * 4);
    canvas->surface.width = infoHeader->width;
    canvas->surface.height = infoHeader->height;

	DrawBitmapImage(0, 0, infoHeader->width, infoHeader->height, imageBuffer, &canvas->surface);
    
    canvas->ResetScrollbars();

    free(imageBuffer);
    fclose(image);
}

void OnOpen(){
    LoadImage(FileDialog("/initrd"));
}


int main(int argc, char** argv){
    strcpy(winInfo.title, "LemonPaint");
    window = CreateWindow(&winInfo);

    canvas = new Canvas({{80,0},{656, 496}}, {640, 480});
    memset(canvas->surface.buffer, 255, 640*480*4);

    Brush* brush = new Brush();
    brush->data = (surface_t){.x = 0, .y = 0, .width = 1, .height = 1, .buffer = brush0};
    brushes.add_back(brush);
    canvas->currentBrush = brush;

    window->widgets.add_back(canvas);
    
    brush = new Brush();
    brush->data = (surface_t){.x = 0, .y = 0, .width = 8, .height = 8, .buffer = brush1};
    brushes.add_back(brush);

    int yPos = 0;
    for(int i = 0; i < DEFAULT_COLOUR_COUNT / 2; i++){
        Colour* c1 = new Colour((rect_t){{2, yPos}, {16, 16}});
        c1->colour = defaultColours[i * 2];
        window->widgets.add_back(c1);

        Colour* c2 = new Colour((rect_t){{20, yPos}, {16, 16}});
        c2->colour = defaultColours[i * 2 + 1];
        window->widgets.add_back(c2);

        yPos += 18;
    }

    Button* openButton = new Button("Open...", {{2, yPos}, {76, 24}});
    openButton->OnPress = OnOpen;

    window->widgets.add_back(openButton);

    yPos += 26;
    Button* brush0Button = new Button("Brush 0", {{2, yPos}, {76, 24}});
    brush0Button->OnPress = OnPressBrush0;
    window->widgets.add_back(brush0Button);

    yPos += 26;
    Button* brush1Button = new Button("Brush 1", {{2, yPos}, {76, 24}});
    brush1Button->OnPress = OnPressBrush1;
    window->widgets.add_back(brush1Button);
    
    yPos += 26;
    Button* scaleButton1 = new Button("Scale 25%", {{2, yPos}, {76, 24}});
    scaleButton1->OnPress = OnPressBrushScale25;
    window->widgets.add_back(scaleButton1);
    
    yPos += 26;
    Button* scaleButton2 = new Button("Scale 50%", {{2, yPos}, {76, 24}});
    scaleButton2->OnPress = OnPressBrushScale50;
    window->widgets.add_back(scaleButton2);
    
    yPos += 26;
    Button* scaleButton3 = new Button("Scale 100%", {{2, yPos}, {76, 24}});
    scaleButton3->OnPress = OnPressBrushScale100;
    window->widgets.add_back(scaleButton3);
    
    yPos += 26;
    Button* scaleButton4 = new Button("Scale 150%", {{2, yPos}, {76, 24}});
    scaleButton4->OnPress = OnPressBrushScale150;
    window->widgets.add_back(scaleButton4);
    
    yPos += 26;
    Button* scaleButton5 = new Button("Scale 200%", {{2, yPos}, {76, 24}});
    scaleButton5->OnPress = OnPressBrushScale200;
    window->widgets.add_back(scaleButton5);
    
	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = (msg.data >> 32);
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseDown(window, {(int)mouseX, (int)mouseY});
			}
			else if(msg.msg == WINDOW_EVENT_MOUSEUP){	
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				HandleMouseUp(window, {(int)mouseX, (int)mouseY});
			} else if (msg.msg == WINDOW_EVENT_MOUSEMOVE) {
				uint32_t mouseX = msg.data >> 32;
				uint32_t mouseY = msg.data & 0xFFFFFFFF;

				HandleMouseMovement(window, {mouseX, mouseY});
			} else if (msg.msg == WINDOW_EVENT_CLOSE) {
				DestroyWindow(window);
				exit(0);
			}
		}

		PaintWindow(window);
	}
}
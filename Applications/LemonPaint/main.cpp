#include "paint.h"

#include <gui/window.h>
#include <gui/widgets.h>
#include <gui/messagebox.h>
#include <stdio.h>
#include <stdlib.h>
#include <gui/filedialog.h>

#define BRUSH_CALLBACK(x) void OnPressBrush ## x (Lemon::GUI::Button* button)
#define SCALE_CALLBACK(x) void OnPressBrushScale ## x (Lemon::GUI::Button* button)

Lemon::GUI::Window* window;
Canvas* canvas;
std::vector<Brush*> brushes;

void Colour::Paint(surface_t* surface){
    Lemon::Graphics::DrawRect(bounds, colour, surface);
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

BRUSH_CALLBACK(0) { canvas->currentBrush = brushes[0]; }
BRUSH_CALLBACK(1) { canvas->currentBrush = brushes[1]; }

SCALE_CALLBACK(25) { canvas->brushScale = 0.01 * 25; }
SCALE_CALLBACK(50) { canvas->brushScale = 0.01 * 50; }
SCALE_CALLBACK(100) { canvas->brushScale = 0.01 * 100; }
SCALE_CALLBACK(150) { canvas->brushScale = 0.01 * 150; }
SCALE_CALLBACK(200) { canvas->brushScale = 0.01 * 200; }

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
        Lemon::GUI::DisplayMessageBox("LemonPaint", "Invalid Filepath");
        return;
    }

    if(canvas->surface.buffer){
        free(canvas->surface.buffer);
    }

    Lemon::Graphics::LoadImage(path, &canvas->surface);
    
    canvas->ResetScrollbars();
}

void SaveImage(char* path){
    if(!path){
        Lemon::GUI::DisplayMessageBox("LemonPaint", "Could not save: Invalid Filepath");
        return;
    }

    char* ext = strrchr(path, '.');
    if(!ext || strcmp(ext, ".png")){
        Lemon::GUI::DisplayMessageBox("LemonPaint", "Could not save: Can only save images in PNG format!");
        return;
    }

    FILE* image = fopen(path, "wb");
    if(!image){
        Lemon::GUI::DisplayMessageBox("LemonPaint", "Failed to open file for writing!");
        return;
    }

    Lemon::Graphics::SavePNGImage(image, &canvas->surface, true);
}

void OnOpen(Lemon::GUI::Button* btn){
    LoadImage(Lemon::GUI::FileDialog("/"));
}

void OnSave(Lemon::GUI::Button* btn){
    SaveImage(Lemon::GUI::FileDialog("/", FILE_DIALOG_CREATE));
}

int main(int argc, char** argv){
    window = new Lemon::GUI::Window("LemonPaint", {800, 500}, 0, Lemon::GUI::WindowType::GUI);

    canvas = new Canvas({{80,0},{656, 496}}, {640, 480});
    memset(canvas->surface.buffer, 0, 640*480*4);

    Brush* brush = new Brush();
    brush->data = (surface_t){.width = 1, .height = 1, .depth = 32, .buffer = brush0};
    brushes.push_back(brush);
    canvas->currentBrush = brush;

    window->AddWidget(canvas);
    
    brush = new Brush();
    brush->data = (surface_t){.width = 8, .height = 8, .depth = 32, .buffer = brush1};
    brushes.push_back(brush);

    int yPos = 0;
    for(int i = 0; i < DEFAULT_COLOUR_COUNT / 2; i++){
        Colour* c1 = new Colour((rect_t){{2, yPos}, {16, 16}});
        c1->colour = defaultColours[i * 2];
        window->AddWidget(c1);

        Colour* c2 = new Colour((rect_t){{20, yPos}, {16, 16}});
        c2->colour = defaultColours[i * 2 + 1];
        window->AddWidget(c2);

        yPos += 18;
    }

    Lemon::GUI::Button* openButton = new Lemon::GUI::Button("Open...", {{2, yPos}, {76, 24}});
    openButton->OnPress = OnOpen;

    window->AddWidget(openButton);

    yPos += 26;
    Lemon::GUI::Button* saveButton = new Lemon::GUI::Button("Save...", {{2, yPos}, {76, 24}});
    saveButton->OnPress = OnSave;
    window->AddWidget(saveButton);

    yPos += 26;
    Lemon::GUI::Button* brush0Button = new Lemon::GUI::Button("Brush 0", {{2, yPos}, {76, 24}});
    brush0Button->OnPress = OnPressBrush0;
    window->AddWidget(brush0Button);

    yPos += 26;
    Lemon::GUI::Button* brush1Button = new Lemon::GUI::Button("Brush 1", {{2, yPos}, {76, 24}});
    brush1Button->OnPress = OnPressBrush1;
    window->AddWidget(brush1Button);
    
    yPos += 26;
    Lemon::GUI::Button* scaleButton1 = new Lemon::GUI::Button("Scale 25%", {{2, yPos}, {76, 24}});
    scaleButton1->OnPress = OnPressBrushScale25;
    window->AddWidget(scaleButton1);
    
    yPos += 26;
    Lemon::GUI::Button* scaleButton2 = new Lemon::GUI::Button("Scale 50%", {{2, yPos}, {76, 24}});
    scaleButton2->OnPress = OnPressBrushScale50;
    window->AddWidget(scaleButton2);
    
    yPos += 26;
    Lemon::GUI::Button* scaleButton3 = new Lemon::GUI::Button("Scale 100%", {{2, yPos}, {76, 24}});
    scaleButton3->OnPress = OnPressBrushScale100;
    window->AddWidget(scaleButton3);
    
    yPos += 26;
    Lemon::GUI::Button* scaleButton4 = new Lemon::GUI::Button("Scale 150%", {{2, yPos}, {76, 24}});
    scaleButton4->OnPress = OnPressBrushScale150;
    window->AddWidget(scaleButton4);
    
    yPos += 26;
    Lemon::GUI::Button* scaleButton5 = new Lemon::GUI::Button("Scale 200%", {{2, yPos}, {76, 24}});
    scaleButton5->OnPress = OnPressBrushScale200;
    window->AddWidget(scaleButton5);
    
	while(!window->closed){
        Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
            window->GUIHandleEvent(ev);
		}

        window->Paint();

        window->WaitEvent();
	}

    return 0;
}
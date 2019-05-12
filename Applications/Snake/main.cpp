#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <core/keyboard.h>
#include <core/ipc.h>
#include <stdlib.h>
#include <list.h>

#define SNAKE_CELL_EMPTY 0
#define SNAKE_CELL_SNAKE 1
#define SNAKE_CELL_APPLE 2
#define SNAKE_CELL_LEMON 3

rgba_colour_t snakeCellColours[]{
	{96,128,96,255},
	{64,64,64,255},
	{64,64,64,255},
	{64,64,64,255},
};

uint32_t frameWaitTime = 100; // For 10 FPS (in ticks)

uint32_t timerFrequency = 1000; // Timer frequency (Hz)

uint64_t lastUptimeSeconds;
uint32_t lastUptimeTicks;

uint32_t tickCounter;

bool gameOver;

unsigned long int rand_next = 1;

unsigned int rand()
{
	rand_next = rand_next * 1103515245 + 12345;
	return ((unsigned int)(rand_next / 65536) % 32768);
}

void Wait(){
	while(tickCounter < frameWaitTime){
		uint32_t seconds;
		uint32_t ticks;

		syscall(SYS_UPTIME, (uint32_t)&seconds, (uint32_t)&ticks,0,0,0);

		tickCounter += (seconds - lastUptimeSeconds)*1000 + (ticks - lastUptimeTicks);
		lastUptimeSeconds = seconds;
		lastUptimeTicks = ticks;
	}
	tickCounter = 0;
}

List<vector2i_t>* snake;
	
uint8_t snakeMapCells[16][16];

void Reset(){
	//delete snake;
	snake = new List<vector2i_t>();
	
	snake->add_back({8,8});
	snake->add_back({9,8});
}

vector2i_t applePos = {1,1};

extern "C"
void pmain(){
	win_info_t windowInfo;
	//handle_t windowHandle;
	Window* window;

	windowInfo.width = 256;
	windowInfo.height = 256;
	windowInfo.x = 64;
	windowInfo.y = 0;
	windowInfo.flags = 0;

	/*surface_t windowSurface;
	windowSurface.buffer = (uint8_t*)malloc(256*256*4);
	windowSurface.width = 256;
	windowSurface.height = 256;*/

	snake = new List<vector2i_t>();

	snake->add_back({8,8});
	snake->add_back({9,8});

	for(int i = 0; i < 16; i++){
		for(int j = 0; j < 16; j++){
			snakeMapCells[i][j] = SNAKE_CELL_EMPTY;
		}
	}

	window = CreateWindow(&windowInfo);

	int direction;

	for(;;){
		Wait();

		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY){
				if(gameOver) {
					gameOver = false;
					Reset();
				}
				switch(msg.data){
					case KEY_ARROW_UP:
						direction = 0;
						break;
					case KEY_ARROW_LEFT:
						direction = 1;
						break;
					case KEY_ARROW_RIGHT:
						direction = 2;
						break;
					case KEY_ARROW_DOWN:
						direction = 3;
						break;
				}
			}
		}
		
		if(gameOver) continue;

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 16; j++){
				snakeMapCells[i][j] = SNAKE_CELL_EMPTY;
			}
		}

		snakeMapCells[applePos.x][applePos.y] = SNAKE_CELL_APPLE;

		for(int i = 0; i < snake->get_length();i++){
			snakeMapCells[snake->get_at(i).x][snake->get_at(i).y] = SNAKE_CELL_SNAKE; 
		}

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 16; j++){
				DrawRect(i*16, j*16, 16, 16, snakeCellColours[snakeMapCells[i][j]].r, snakeCellColours[snakeMapCells[i][j]].g, snakeCellColours[snakeMapCells[i][j]].b, &window->surface);
			}
		}

		switch(direction){
			case 0:
				snake->remove_at(snake->get_length()-1);
				snake->add_front({snake->get_at(0).x, snake->get_at(0).y - 1});
				break;
			case 1:
				snake->remove_at(snake->get_length()-1);
				snake->add_front({snake->get_at(0).x-1, snake->get_at(0).y});
				break;
			case 2:
				snake->remove_at(snake->get_length()-1);
				snake->add_front({snake->get_at(0).x+1, snake->get_at(0).y});
				break;
			case 3:
				snake->remove_at(snake->get_length()-1);
				snake->add_front({snake->get_at(0).x, snake->get_at(0).y + 1});
				break;
			}

		if(snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] == SNAKE_CELL_SNAKE || snake->get_at(0).y < 0 || snake->get_at(0).x < 0 || snake->get_at(0).y >= 16 || snake->get_at(0).x >= 16){
			gameOver = true;
			DrawString("Game Over, Press ENTER to Reset", 0, 0, 255, 255, 255, &window->surface);
			syscall(SYS_PAINT_WINDOW, (uint32_t)window->handle, (uint32_t)&window->surface,0,0,0);
			continue;
		} else if(snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] == SNAKE_CELL_APPLE){
			snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] = SNAKE_CELL_EMPTY;
			
			applePos = { rand() % 16, rand() % 16 };

			snake->add_back({0,0});
		}
		syscall(SYS_PAINT_WINDOW, (uint32_t)window->handle, (uint32_t)&window->surface,0,0,0);
	}

	for(;;);
}

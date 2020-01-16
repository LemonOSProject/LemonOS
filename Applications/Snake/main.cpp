#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <lemon/keyboard.h>
#include <lemon/ipc.h>
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
	{64,64,0,255},
};

rgba_colour_t bgColourDefault = {96,128,96,255};

uint64_t frameWaitTime = 100; // For ~10 FPS (in ms)
uint64_t frameWaitTimeDefault = 100;

int powerUp = 0;

uint64_t lastUptimeSeconds;
uint64_t lastUptimeMs;

uint64_t msCounter;

bool gameOver = true;

unsigned long int rand_next = 1;

unsigned int rand()
{
	rand_next = rand_next * 1103515245 + 12345;
	return ((unsigned int)(rand_next / 65536) % 32768);
}

void Wait(){
	while(msCounter < frameWaitTime){
		asm("hlt");
		uint64_t seconds;
		uint64_t milliseconds;

		syscall(SYS_UPTIME, (uint64_t)&seconds, (uint64_t)&milliseconds,0,0,0);

		msCounter += (seconds - lastUptimeSeconds)*1000 + (milliseconds - lastUptimeMs);
		lastUptimeSeconds = seconds;
		lastUptimeMs = milliseconds;
	}
	msCounter = 0;
}

List<vector2i_t>* snake;
	
uint8_t snakeMapCells[16][16];
int powerUpTimer;
int powerUpTimerDefault = 100;
int fruitType = 0; // 0 - Apple, 1 - Lemon

void Reset(){
	snake->clear();
	//delete snake;
	//snake = new List<vector2i_t>();
	
	snake->add_back({8,8});
	snake->add_back({9,8});

	powerUp = 0;
	frameWaitTime = frameWaitTimeDefault;
	snakeCellColours[0] = bgColourDefault;
}

vector2i_t applePos = {1,1};

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;
	//handle_t windowHandle;
	Window* window;

	windowInfo.width = 256;
	windowInfo.height = 256;
	windowInfo.x = 64;
	windowInfo.y = 0;
	windowInfo.flags = 0;

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

	Reset();

	for(;;){
		Wait();

		if(powerUp){
			if(powerUpTimer){
				powerUpTimer--;
				snakeCellColours[0] = {255, 0, 0, 255};
			} else {
				powerUp = 0;
				frameWaitTime = frameWaitTimeDefault;
				snakeCellColours[0] = bgColourDefault;
			}
		}

		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.msg == WINDOW_EVENT_KEY){
				if(gameOver) {
					gameOver = false;
					Reset();
					continue;
				}
				switch(msg.data){
					case 'w':
					case KEY_ARROW_UP:
						direction = 0;
						break;
					case 'a':
					case KEY_ARROW_LEFT:
						direction = 1;
						break;
					case 'd':
					case KEY_ARROW_RIGHT:
						direction = 2;
						break;
					case 's':
					case KEY_ARROW_DOWN:
						direction = 3;
						break;
				}
			} else if (msg.msg == WINDOW_EVENT_KEYRELEASED && gameOver) {
				gameOver = false;
				Reset();
				continue;
			} else if (msg.msg == WINDOW_EVENT_CLOSE){
				DestroyWindow(window);
				exit();
			}
		}
		
		if(gameOver) continue;

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 16; j++){
				snakeMapCells[i][j] = SNAKE_CELL_EMPTY;
			}
		}

		snakeMapCells[applePos.x][applePos.y] = fruitType ? SNAKE_CELL_LEMON : SNAKE_CELL_APPLE;

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
			DrawString("Game Over, Press any key to Reset", 0, 0, 255, 255, 255, &window->surface);
			_PaintWindow(window->handle, &window->surface);
			//syscall(SYS_UPDATE_WINDOW, (uint64_t)window->handle, (uint64_t)&window->surface,0,0,0);
			continue;
		} else if(snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] == SNAKE_CELL_APPLE){
			snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] = SNAKE_CELL_EMPTY;
			
			applePos = { rand() % 16, rand() % 16 };
			if(!(rand() % 4))
				fruitType = 1;
			else fruitType = 0;

			snake->add_back({0,0});
		} else if(snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] == SNAKE_CELL_LEMON){
			snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] = SNAKE_CELL_EMPTY;
			
			powerUp = 1;
			powerUpTimer = powerUpTimerDefault;
			frameWaitTime /= 2;

			applePos = { rand() % 16, rand() % 16 };
			if(!(rand() % 3))
				fruitType = 1;
			else fruitType = 0;

			snake->add_back({0,0});
		}
		//syscall(SYS_UPDATE_WINDOW, (uint64_t)window->handle, (uint64_t)&window->surface,0,0,0);
		_PaintWindow(window->handle, &window->surface);
	}

	for(;;);
}
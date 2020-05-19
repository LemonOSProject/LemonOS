#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/window.h>
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

uint64_t frameWaitTime = 90; // For ~10 FPS (in ms)
uint64_t frameWaitTimeDefault = 90;

int powerUp = 0;

uint64_t lastUptimeSeconds;
uint64_t lastUptimeMs;

uint64_t msCounter;

bool gameOver = true;

unsigned long int rand_next = 1;

unsigned int snakeRand()
{
	rand_next = rand_next * 1103515245 + 12345;
	return ((unsigned int)(rand_next / 65536) % 32768);
}

void Wait(){
	while(msCounter < frameWaitTime){
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

int direction;
	
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

	gameOver = false;

	powerUp = 0;
	frameWaitTime = frameWaitTimeDefault;
	snakeCellColours[0] = bgColourDefault;

	direction = 0;

	for(int i = 0; i < 16; i++){
		for(int j = 0; j < 16; j++){
			snakeMapCells[i][j] = SNAKE_CELL_EMPTY;
		}
	}

	syscall(SYS_UPTIME, (uintptr_t)&lastUptimeSeconds, (uintptr_t)&lastUptimeMs, 0,0,0 );
}

vector2i_t applePos = {1,1};

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;
	Lemon::GUI::Window* window;

	windowInfo.width = 256;
	windowInfo.height = 256;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "Snake");

	snake = new List<vector2i_t>();

	window = Lemon::GUI::CreateWindow(&windowInfo);

	Reset();

	for(;;){

		Wait();

		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			if(msg.msg == WINDOW_EVENT_KEY){
				if(gameOver) {
					gameOver = false;
					Reset();
					continue;
				}
				switch(msg.data){
					case 'w':
					case KEY_ARROW_UP:
						if(direction != 3)
							direction = 0;
						break;
					case 'a':
					case KEY_ARROW_LEFT:
						if(direction != 2)
							direction = 1;
						break;
					case 'd':
					case KEY_ARROW_RIGHT:
						if(direction != 1)
							direction = 2;
						break;
					case 's':
					case KEY_ARROW_DOWN:
						if(direction != 0)
							direction = 3;
						break;
				}
			} else if (msg.msg == WINDOW_EVENT_KEYRELEASED && gameOver) {
				gameOver = false;
				Reset();
				continue;
			} else if (msg.msg == WINDOW_EVENT_CLOSE){
				Lemon::GUI::DestroyWindow(window);
				exit(0);
			}
		}
		
		if(gameOver) continue;

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

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 16; j++){
				snakeMapCells[i][j] = SNAKE_CELL_EMPTY;
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
			Lemon::Graphics::DrawString("Game Over, Press any key to Reset", 0, 0, 255, 255, 255, &window->surface);
			Lemon::GUI::SwapWindowBuffers(window);
			continue;
		} else if(snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] == SNAKE_CELL_APPLE){
			snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] = SNAKE_CELL_EMPTY;
			
			applePos = { snakeRand() % 16, snakeRand() % 16 };
			if(!(snakeRand() % 4))
				fruitType = 1;
			else fruitType = 0;

			snake->add_back({0,0});
		} else if(snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] == SNAKE_CELL_LEMON){
			snakeMapCells[snake->get_at(0).x][snake->get_at(0).y] = SNAKE_CELL_EMPTY;
			
			powerUp = 1;
			powerUpTimer = powerUpTimerDefault;
			frameWaitTime /= 2;

			applePos = { snakeRand() % 16, snakeRand() % 16 };
			if(!(snakeRand() % 3))
				fruitType = 1;
			else fruitType = 0;

			snake->add_back({0,0});
		}

		snakeMapCells[applePos.x][applePos.y] = fruitType ? SNAKE_CELL_LEMON : SNAKE_CELL_APPLE;

		for(int i = 0; i < snake->get_length();i++){
			snakeMapCells[snake->get_at(i).x][snake->get_at(i).y] = SNAKE_CELL_SNAKE; 
		}

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 16; j++){
				Lemon::Graphics::DrawRect(i*16, j*16, 16, 16, snakeCellColours[snakeMapCells[i][j]].r, snakeCellColours[snakeMapCells[i][j]].g, snakeCellColours[snakeMapCells[i][j]].b, &window->surface);
			}
		}
		
		Lemon::GUI::SwapWindowBuffers(window);
	}

	for(;;);
}

#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gui/window.h>
#include <core/keyboard.h>
#include <stdlib.h>
#include <list>

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

std::list<vector2i_t> snake;

int direction;
	
uint8_t snakeMapCells[16][16];

void Reset(){
	snake.clear();
	//delete snake;
	//snake = new List<vector2i_t>();
	
	snake.push_back({8,8});
	snake.push_back({9,8});

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

int main(){
	Lemon::GUI::Window* window = new Lemon::GUI::Window("Snake", {256, 256});

	Reset();

	for(;;){

		Wait();

		Lemon::LemonEvent msg;
		while(window->PollEvent(msg)){
			if(msg.event == Lemon::EventKeyPressed){
				if(gameOver) {
					gameOver = false;
					Reset();
					continue;
				}
				switch(msg.key){
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
			} else if (msg.event == Lemon::EventKeyReleased && gameOver) {
				gameOver = false;
				Reset();
				continue;
			} else if (msg.event == Lemon::EventWindowClosed){
				delete window;
				exit(0);
			}
		}
		
		if(gameOver) continue;

		switch(direction){
			case 0:
				snake.pop_back();
				snake.push_front({snake.front().x, snake.front().y - 1});
				break;
			case 1:
				snake.pop_back();
				snake.push_front({snake.front().x - 1, snake.front().y});
				break;
			case 2:
				snake.pop_back();
				snake.push_front({snake.front().x + 1, snake.front().y});
				break;
			case 3:
				snake.pop_back();
				snake.push_front({snake.front().x, snake.front().y + 1});
				break;
			}

		if(snakeMapCells[snake.front().x][snake.front().y] == SNAKE_CELL_SNAKE || snake.front().y < 0 || snake.front().x < 0 || snake.front().y >= 16 || snake.front().x >= 16){
			gameOver = true;
			Lemon::Graphics::DrawString("Game Over, Press any key to Reset", 0, 0, 255, 255, 255, &window->surface);
			window->SwapBuffers();
			continue;
		} else if(snakeMapCells[snake.front().x][snake.front().y] == SNAKE_CELL_APPLE){
			snakeMapCells[snake.front().x][snake.front().y] = SNAKE_CELL_EMPTY;
			
			applePos = { snakeRand() % 16, snakeRand() % 16 };

			snake.push_back({0,0});
		}

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 16; j++){
				snakeMapCells[i][j] = SNAKE_CELL_EMPTY;
			}
		}

		for(vector2i_t& pos : snake){
			snakeMapCells[pos.x][pos.y] = SNAKE_CELL_SNAKE; 
		}

		snakeMapCells[applePos.x][applePos.y] = SNAKE_CELL_APPLE;

		for(int i = 0; i < 16; i++){
			for(int j = 0; j < 16; j++){
				Lemon::Graphics::DrawRect(i*16, j*16, 16, 16, snakeCellColours[snakeMapCells[i][j]].r, snakeCellColours[snakeMapCells[i][j]].g, snakeCellColours[snakeMapCells[i][j]].b, &window->surface);
			}
		}
		
		window->SwapBuffers();
	}

	for(;;);
}

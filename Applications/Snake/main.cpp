#include <Lemon/GUI/Window.h>
#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Core/Keyboard.h>

#include <unistd.h>

#define SNAKE_CELL_SIZE 16
#define SNAKE_MAP_WIDTH 24
#define SNAKE_MAP_HEIGHT 16

Lemon::GUI::Window* win;

std::list<vector2i_t> snake = {{10, 10}, {11, 11}};
vector2i_t apple = {0, 0};

enum {
	DirectionUp,
	DirectionLeft,
	DirectionDown,
	DirectionRight,
} direction;

void Reset(){
	snake = {{10, 10}, {11, 11}};
}

bool gameOver = false;
void GameOverLoop(){
	gameOver = true;

	while(!win->closed && gameOver){
		Lemon::LemonEvent ev;
		while(win->PollEvent(ev)){
			if(ev.event == Lemon::EventKeyReleased){
				gameOver = false;
				break;
			} else if(ev.event == Lemon::EventWindowClosed){
				delete win;

				exit(0);
			}
		}
	}
}

void OnPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0, 0, surface->width, surface->height, RGBAColour::FromRGB(0x22211f), surface);

	for(const auto& segment : snake){
		Lemon::Graphics::DrawRect(segment.x * SNAKE_CELL_SIZE, segment.y * SNAKE_CELL_SIZE, SNAKE_CELL_SIZE, SNAKE_CELL_SIZE, { 250, 250, 250, 255 }, surface);
	}

	Lemon::Graphics::DrawRect({ {apple.x * SNAKE_CELL_SIZE, apple.y * SNAKE_CELL_SIZE}, { SNAKE_CELL_SIZE, SNAKE_CELL_SIZE } }, { 250, 250, 250, 255 }, surface);
}

int main(){
	win = new Lemon::GUI::Window("Snake", { SNAKE_CELL_SIZE * SNAKE_MAP_WIDTH, SNAKE_CELL_SIZE * SNAKE_MAP_HEIGHT });
	win->OnPaint = OnPaint;

	srand(time(nullptr));

	apple = { rand() % SNAKE_MAP_WIDTH, rand() % SNAKE_MAP_HEIGHT };

	while(!win->closed){
		Lemon::LemonEvent ev;
		while(win->PollEvent(ev)){
			if(ev.event == Lemon::EventKeyPressed){
				switch(ev.key){
				case 'w':
				case 'W':
				case KEY_ARROW_UP:
					direction = DirectionUp;
					break;
				case 'd':
				case 'D':
				case KEY_ARROW_RIGHT:
					direction = DirectionRight;
					break;
				case 's':
				case 'S':
				case KEY_ARROW_DOWN:
					direction = DirectionDown;
					break;
				case 'a':
				case 'A':
				case KEY_ARROW_LEFT:
					direction = DirectionLeft;
					break;
				}
			} else if(ev.event == Lemon::EventWindowClosed){
				delete win;

				exit(0);
			}
		}

		snake.pop_back();
		switch(direction){
		case DirectionUp:
			if(snake.front().y <= 0){
				gameOver = true;
				goto gOver;
			}

			snake.push_front({ snake.front().x, snake.front().y - 1 });
			break;
		case DirectionLeft:
			if(snake.front().x <= 0){
				gameOver = true;
				goto gOver;
			}

			snake.push_front({ snake.front().x - 1, snake.front().y });
			break;
		case DirectionDown:
			if(snake.front().y >= SNAKE_MAP_HEIGHT){
				gameOver = true;
				goto gOver;
			}

			snake.push_front({ snake.front().x, snake.front().y + 1 });
			break;
		case DirectionRight:
			if(snake.front().x >= SNAKE_MAP_WIDTH){
				gameOver = true;
				goto gOver;
			}

			snake.push_front({ snake.front().x + 1, snake.front().y });
			break;
		}

		if(snake.front() == apple){
			snake.push_back(snake.back()); // Add segment to snake
			apple = { rand() % SNAKE_MAP_WIDTH, rand() % SNAKE_MAP_HEIGHT };
		}

		win->Paint();
		usleep(100000);

	gOver:
		if(gameOver){
			GameOverLoop();
			Reset();
		}
	}

	delete win;
	return 0;
}
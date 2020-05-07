#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

#include <stdio.h>
#include <math.h>

int main(int argc, char * argv[])
{
	if(SDL_Init(SDL_INIT_VIDEO)){
		printf("Error initializing video: %s", SDL_GetError());
		return 1;
	}
	
	SDL_Window* window = SDL_CreateWindow("SDL Test", 10, 10, 640, 480, 0);
	
	if(!window)
	{
		printf("Error creating window: %s", SDL_GetError());
		return 1;
	}

	SDL_Surface* windowSurface = SDL_GetWindowSurface(window);
	for(;;){

		SDL_FillRect(windowSurface, NULL, SDL_MapRGB( windowSurface->format, rand() % 255, rand() % 255, rand() % 255 ) );
		
		SDL_UpdateWindowSurface(window);

		SDL_Delay(1000);
	}

    for(;;);
}
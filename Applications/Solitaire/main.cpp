#include <lemon/types.h>
#include <lemon/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <lemon/keyboard.h>
#include <lemon/ipc.h>
#include <stdlib.h>
#include <list.h>
#include <math.h>

#define RANK_ACE 1

#define RANK_JACK 11
#define RANK_QUEEN 12
#define RANK_KING 13

#define CARD_WIDTH 60
#define CARD_HEIGHT 90

enum Suits{
	Suit_Hearts,
	Suit_Diamonds,
	Suit_Clubs,
	Suit_Spades,
};

struct Card{
	int suit;
	int rank;
	bool flipped;
};

List<Card> stock;
List<Card> tableau[7];
List<Card> foundations[4];

int stockIndex;

void RefreshCards(){
	for(int i = 0; i < 7; i++){
		Card card = tableau[i].get_at(tableau[i].get_length() - 1);
		card.flipped = true;
		tableau[i].replace_at(tableau[i].get_length() - 1, card);
	}
}

void OnPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0, 0, surface->width, surface->height, 10, 120, 10, surface);

	for(int i = 0; i < 7; i++){
		for(int j = 0; j < tableau[i].get_length(); j++){
			Card c = tableau[i][j];
			if(c.flipped){
				Lemon::Graphics::DrawRect(i * (CARD_WIDTH + 8) + 20, j * CARD_HEIGHT / 2 + 100, CARD_WIDTH, CARD_HEIGHT, 220, 220, 220, surface);
				char _rank[3];
				_rank[2] = 0;
				_rank[1] = 0;
				switch (c.rank)
				{
				case RANK_ACE:
					*_rank = 'A';
					break;
				case RANK_JACK:
					*_rank = 'J';
					break;
				case RANK_QUEEN:
					*_rank = 'Q';
					break;
				case RANK_KING:
					*_rank = 'K';
					break;
				case 10:
					_rank[0] = '1';
					_rank[1] = '0';
				default:
					_rank[0] = c.rank + '0';
					break;
				}
				Lemon::Graphics::DrawString(_rank, i * (CARD_WIDTH + 8) + 20 + 3, j * CARD_HEIGHT / 2 + 100 + 3, (c.suit <= 2) ?  255 : 0, 0, 0, surface);;
			} else {
				Lemon::Graphics::DrawRect(i * (CARD_WIDTH + 8) + 20, j * CARD_HEIGHT / 2 + 100, CARD_WIDTH, CARD_HEIGHT, 200, 20, 20, surface);
			}
		}
	}
}

int main(char argc, char** argv){
	win_info_t windowInfo;
	Lemon::GUI::Window* window;

	windowInfo.width = 600;
	windowInfo.height = 450;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "Solitaire");

	window = Lemon::GUI::CreateWindow(&windowInfo);
	window->OnPaint = OnPaint;

	for(int i = 1; i <= 4; i++){
		for(int j = 1; j <= RANK_KING; j++){
			Card c;
			c.suit = i;
			c.rank = j;
			stock.add_back(c);
		}
	}

	for(int i = 0; i < 200; i++){ // Shuffle cards 200 times
		stock.add_back(stock.remove_at(rand() % (stock.get_length()-1)));
	}

	for(int i = 0; i < 7; i++){
		for(int j = 0; j < i; j++){
			tableau[i].add_back(stock.remove_at(0));
		}
	}

	RefreshCards();

	for(;;){
		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_CLOSE){
				//DestroyWindow(window);
				//exit(0);
			}
		}
		
		Lemon::GUI::PaintWindow(window);
	}

	for(;;);
}
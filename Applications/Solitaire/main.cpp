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

#define CARD_WIDTH 80
#define CARD_HEIGHT 110

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

List<Card*> stock;
List<Card*> tableau[7];
List<Card*> foundations[4];

int tableauCardGap = 8;
rect_t stockRect = {{5, 5}, {CARD_WIDTH, CARD_HEIGHT}};
rect_t stockRectFlipped = {{5 + CARD_WIDTH + tableauCardGap, 5}, {CARD_WIDTH, CARD_HEIGHT}};

surface_t resources;
rect_t cardBack = {{0, 0}, {CARD_WIDTH, CARD_HEIGHT}};
rect_t spades = {{CARD_WIDTH, 0}, {16, 16}};
rect_t clubs = {{CARD_WIDTH, 16}, {16, 16}};
rect_t diamonds = {{CARD_WIDTH, 32}, {16, 16}};
rect_t hearts = {{CARD_WIDTH, 48}, {16, 16}};
vector2i_t tableauOffset = {5, (CARD_HEIGHT + 10) / 2};

rgba_colour_t cardFrontColour = {255, 255, 255, 255};

int stockIndex = 0;

void RefreshCards(){
	for(int i = 0; i < 7; i++){
		Card* card = tableau[i].get_at(tableau[i].get_length() - 1);
		card->flipped = true;
		tableau[i].replace_at(tableau[i].get_length() - 1, card);
	}
}

void DrawCard(surface_t* surface, Card* c, rect_t rect){
		if(c->flipped){
			Lemon::Graphics::DrawRect(rect, cardFrontColour, surface);
			char _rank[3];
			_rank[2] = 0;
			_rank[1] = 0;
			switch (c->rank)
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
				_rank[0] = c->rank + '0';
				break;
			}
			Lemon::Graphics::DrawString(_rank, rect.pos.x + 3, rect.pos.y, (c->suit <= 2) ?  255 : 0, 0, 0, surface);;
			rect_t symbol;
			switch(c->suit){
				case Suit_Clubs:
					symbol = clubs;
					break;
				case Suit_Diamonds:
					symbol = diamonds;
					break;
				case Suit_Hearts:
					symbol = hearts;
					break;
				case Suit_Spades:
				default:
					symbol = spades;
					break;
			}
			Lemon::Graphics::surfacecpy(surface, &resources, rect.pos + (vector2i_t){3, 18}, symbol);
		} else {
			Lemon::Graphics::surfacecpy(surface, &resources, rect.pos, cardBack);
		}
		Lemon::Graphics::DrawRect(rect.pos.x, rect.pos.y, 1, rect.size.y, 0, 0, 0, surface);
		Lemon::Graphics::DrawRect(rect.pos.x + CARD_WIDTH - 1, rect.pos.y, 1, rect.size.y, 0, 0, 0, surface);
		Lemon::Graphics::DrawRect(rect.pos.x, rect.pos.y, rect.size.x, 1, 0, 0, 0, surface);
		Lemon::Graphics::DrawRect(rect.pos.x, rect.pos.y + CARD_HEIGHT - 1, rect.size.x, 1, 0, 0, 0, surface);
}

void OnPaint(surface_t* surface){
	Lemon::Graphics::DrawRect(0, 0, surface->width, surface->height, 10, 120, 10, surface);

	for(int i = 0; i < 7; i++){
		for(int j = 0; j < tableau[i].get_length(); j++){
			Card* c = tableau[i][j];
			rect_t card = {tableauOffset + (vector2i_t){i * (CARD_WIDTH + tableauCardGap), j * (CARD_HEIGHT / 3) + tableauOffset.y}, CARD_WIDTH, CARD_HEIGHT};
			DrawCard(surface, c, card);
		}
	}

	if(stockIndex < stock.get_length() - 1){
		Lemon::Graphics::surfacecpy(surface, &resources, stockRect.pos, cardBack);
	}
	Card c = *stock[stockIndex];
	c.flipped = true;
	DrawCard(surface, &c, stockRectFlipped);
}

void MouseDown(vector2i_t mousePos){

}

void MouseUp(vector2i_t mousePos){
	
}

int main(int argc, char** argv){
	win_info_t windowInfo;
	Lemon::GUI::Window* window;

	windowInfo.width = (CARD_WIDTH + tableauCardGap) * 7 + 20;
	windowInfo.height = 500;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "Solitaire");

	Lemon::Graphics::LoadImage("/initrd/solitaire.bmp", &resources);

	window = Lemon::GUI::CreateWindow(&windowInfo);
	window->OnPaint = OnPaint;

	for(int i = 0; i <= 4; i++){
		for(int j = 1; j <= RANK_KING; j++){
			Card* c = new Card;
			c->suit = i;
			c->rank = j;
			stock.add_back(c);
		}
	}

	for(int i = 0; i < 200; i++){ // Shuffle cards 200 times
		stock.add_back(stock.remove_at(rand() % (stock.get_length()-2)));
	}

	for(int i = 0; i < 7; i++){
		for(int j = 0; j < i + 1; j++){
			tableau[i].add_back(stock[rand() % (stock.get_length()-2)]);
		}
	}

	RefreshCards();

	for(;;){
		ipc_message_t msg;
		while(Lemon::ReceiveMessage(&msg)){
			if (msg.msg == WINDOW_EVENT_CLOSE){
				DestroyWindow(window);
				exit(0);
			} else if (msg.msg == WINDOW_EVENT_MOUSEDOWN){
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				MouseDown({(int)mouseX, (int)mouseY});
			} else if (msg.msg == WINDOW_EVENT_MOUSEUP){
				uint32_t mouseX;
				uint32_t mouseY;
				mouseX = msg.data >> 32;
				mouseY = (uint32_t)msg.data & 0xFFFFFFFF;
				MouseUp({(int)mouseX, (int)mouseY});
			}
		}
		
		Lemon::GUI::PaintWindow(window);
	}

	for(;;);
}
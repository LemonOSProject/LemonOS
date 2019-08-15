#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <gfx/window/widgets.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>
#include <list.h>
#include <math.h>

#define CARD_WIDTH 40
#define CARD_HEIGHT 60

#define SPACING 10
#define TABLEAU_CARD_SPACING 20

extern "C"
void __cxa_atexit(){

}

enum CardSuit{
	SuitDiamonds,
	SuitHearts,
	SuitClubs,
	SuitSpades,
};

enum CardType{
	CardAce = 1,
	Card2 = 2,
	Card3 = 3,
	Card4 = 4,
	Card5 = 5,
	Card6 = 6,
	Card7 = 7,
	Card8 = 8,
	Card9 = 9,
	Card10 = 10,
	CardJack = 11,
	CardQueen = 12,
	CardKing = 13,
};

typedef struct{
	uint8_t card;
	uint8_t suit;
	uint8_t drawn;
} Card;

List<Card>* cardList;

List<Card>* stock;
List<Card>* foundations[4];
List<Card>* tableau[7];


vector2i_t mousePos;
vector2i_t stockPos;
vector2i_t stockCardPos;
vector2i_t foundationPositions[4];
vector2i_t tableauPositions[7];

Card grabbedCard;

Card GetFromList(){
	Card card;
	int cardIndex;
	bool isAvailiableCard = false;
	for(int i = 0; i < cardList->get_length(); i++){
		if(!cardList->get_at(i).drawn){
			isAvailiableCard = true;
			break;
		}
	}

	if(!isAvailiableCard) return { 0, 0, 0 };

	while((card = cardList->get_at((cardIndex = (rand() % cardList->get_length())))).drawn);
	card.drawn = true;
	cardList->replace_at(cardIndex, card);
	return card;
}


void Draw(){
	for(int i = 0; i < 7; i++){
		for(int j = 0; j <= i+1; j++){
			tableau[i]->add_back(GetFromList());
		}
	}

	Card card;
	while((card = GetFromList()).card){
		stock->add_back(card);
	}
}

int CheckFoundationBounds(){
	
}

int CheckTableauBounds(){
	if(mousePos.y < tableauPositions[0].y) return -1;
	for(int i = 0; i < 7; i++){
		if(mousePos.x < tableauPositions[i].x || mousePos.x > (tableauPositions[i].x + CARD_WIDTH)) continue;

		if(mousePos.y > tableauPositions[i].y + (tableau[i]->get_length())*TABLEAU_CARD_SPACING + CARD_HEIGHT) continue;

		return i;
	}
	return -1;
}

bool CheckStockBounds(){

}

void OnMouseDown(){
	//if(grabbedCard.card) return;
	int index = CheckTableauBounds();
	if((index) > -1 && index < 7){
		//*(int*)0 = 0;
		Card c = tableau[index]->get_back();
		grabbedCard.suit = c.suit;
		grabbedCard.card = c.card;
		return;
	}
}

void OnMouseUp(){
	return;
	if(grabbedCard.card){
		int index;
		if((index = CheckFoundationBounds())){
			if(grabbedCard.card - (foundations[index]->get_at(foundations[index]->get_length()-1).card) == 1){
				foundations[0]->add_back(grabbedCard);
			}
		}
	}
}

void DrawCard(int x, int y, Card card, surface_t* surface){
	DrawRect(x,y,CARD_WIDTH,CARD_HEIGHT,255,255,255,surface);
	int cardType = card.card;
	if(cardType == 1)
		DrawChar('A', x + 2, y + 12, (card.suit >= SuitClubs) ? 0 : 255, 0, 0, surface);
	if(cardType < 11){
		char temp[3];
		itoa(cardType,temp,10);
		DrawString(temp, x + 2, y + 12, (card.suit >= SuitClubs) ? 0 : 255, 0, 0, surface);
	} else {
		switch (cardType)
		{
		case CardJack:
			DrawChar('J', x + 2, y + 12, (card.suit >= SuitClubs) ? 0 : 255, 0, 0, surface);
			break;
		case CardQueen:
			DrawChar('Q', x + 2, y + 12, (card.suit >= SuitClubs) ? 0 : 255, 0, 0, surface);
			break;
		case CardKing:
			DrawChar('K', x + 2, y + 12, (card.suit >= SuitClubs) ? 0 : 255, 0, 0, surface);
			break;
		}
	}
	DrawRect(x,y,CARD_WIDTH,1,0,0,0,surface);
	DrawRect(x,y,1,CARD_HEIGHT,0,0,0,surface);
	DrawRect(x + CARD_WIDTH, y, 1, CARD_HEIGHT, 0, 0, 0, surface);
	DrawRect(x, y + CARD_HEIGHT, CARD_WIDTH, 1, 0, 0, 0, surface);

	if(grabbedCard.card){
		DrawCard(mousePos.x, mousePos.y, grabbedCard, surface);
	}
}

void OnPaint(surface_t* surface){
	if(stock->get_length() > 1){
		DrawRect(stockPos.x,stockPos.y,CARD_WIDTH,CARD_HEIGHT,255,0,0,surface);
	}
	
	if (stock->get_length() > 0){
		DrawCard(stockCardPos.x,stockCardPos.y,stock->get_at(stock->get_length()-1),surface);
	}

	int xpos = SPACING;
	int ypos = 80;
	for(int i = 0; i < 7; i++){
		ypos = 80;
		for(int j = 0; j < tableau[i]->get_length(); j++){
			if(j == tableau[i]->get_length()-1){ // Dont Draw Flipped
				DrawCard(xpos,ypos, tableau[i]->get_at(j),surface);
			} else {
				DrawRect(xpos,ypos,CARD_WIDTH,CARD_HEIGHT,255,0,0,surface);
				DrawRect(xpos,ypos,CARD_WIDTH,1,0,0,0,surface);
				DrawRect(xpos,ypos,1,CARD_HEIGHT,0,0,0,surface);
				DrawRect(xpos + CARD_WIDTH, ypos, 1, CARD_HEIGHT, 0, 0, 0, surface);
				DrawRect(xpos, ypos + CARD_HEIGHT, CARD_WIDTH, 1, 0, 0, 0, surface);
			}
			ypos += TABLEAU_CARD_SPACING;
		}
		xpos += CARD_WIDTH + SPACING;
	}

	//if(grabbedCard.card)
		DrawCard(mousePos.x, mousePos.y, grabbedCard, surface);

	DrawRect(0, 0, 10, 10, rand() % 256, rand() % 256, 0, surface);
}

int main(int argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 500;
	windowInfo.height = 300;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;
	strcpy(windowInfo.title, "Solitaire");

	Window* win = CreateWindow(&windowInfo);
	win->background = {20,128,20,255};
	win->OnPaint = OnPaint;

	cardList = new List<Card>();

	for(int i = 0; i < 4; i++){
		for(int j = 1; j <= 13; j++){
			Card card;
			card.suit = i;
			card.card = j;
			card.drawn = false;

			cardList->add_back(card);
		}
	}

	Draw();

	stock = new List<Card>();
	for(int i = 0; i < 7; i++){
		tableau[i] = new List<Card>();
		if(i < 4){
			foundations[i] = new List<Card>();
		}
	}

	stockPos = {10,10};
	stockCardPos = {10 + CARD_WIDTH + 10, 10};

	int xpos = SPACING;
	int ypos = 70;
	for(int i = 0; i < 7; i++){
		ypos = 80;
		tableauPositions[i] = {xpos, ypos};
		xpos += CARD_WIDTH + SPACING;
	}

	for(;;){
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if (msg.id == WINDOW_EVENT_MOUSEUP){
				HandleMouseUp(win);
			} else if (msg.id == WINDOW_EVENT_MOUSEDOWN){
				uint16_t mouseX = msg.data >> 16;
				uint16_t mouseY = msg.data & 0xFFFF;
				mousePos = {mouseX, mouseY};
				OnMouseDown();
				HandleMouseDown(win, {mouseX, mouseY});
			} else if (msg.id == WINDOW_EVENT_CLOSE){
				DestroyWindow(win);
				exit();
				for(;;);
			} else if (msg.id == WINDOW_EVENT_KEY){

			}
		}

		PaintWindow(win);
	}
}
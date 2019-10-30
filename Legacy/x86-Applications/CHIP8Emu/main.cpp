#include <core/types.h>
#include <core/syscall.h>
#include <gfx/surface.h>
#include <gfx/graphics.h>
#include <gfx/window/window.h>
#include <stdlib.h>
#include <core/ipc.h>
#include <stdio.h>
#include <core/filesystem.h>
#include <core/keyboard.h>

#define EMU_PIXEL_SIZE 8

uint8_t display[64*32];
uint8_t memory[4096];
uint8_t v[16];

uint16_t instructionPointer = 0x200;
uint16_t indexRegister;
uint16_t stackPointer;

uint8_t keyWaitRegister;

uint8_t key[16];

uint16_t stack[16];

bool drawFlag = false;

unsigned char fontset[80] =
{ 
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

uint32_t frameWaitTime = 18; // For 60 FPS (in ticks)

uint32_t timerFrequency = 10000; // Timer frequency (Hz)

uint64_t lastUptimeSeconds;
uint32_t lastUptimeTicks;

int tickCounter;

float delayTimer;
float soundTimer;
unsigned long int rand_next = 1;

bool emulating = false;

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

		tickCounter += (seconds - lastUptimeSeconds)*timerFrequency + (seconds == lastUptimeSeconds) ? (ticks - lastUptimeTicks) : ticks;
		lastUptimeSeconds = seconds;
		lastUptimeTicks = ticks;
	}
	tickCounter -= frameWaitTime;
	if(tickCounter < 0) tickCounter = 0;
}

char opcodeStringBuffer[16];
char* invalidOpcodeStr = "Invalid Opcode!";
char* str = invalidOpcodeStr;

int romIndex;

bool keyWait;
uint8_t currentKey;

const int romCount = 7;
char* romList[romCount]{
	"/tetris.c8",
	"/spinvade.c8",
	"/pong.c8",
	"/pong2.c8",
	"/missile.c8",
	"/tictac.c8",
	"/ufo.c8",
};

void OnPaint(surface_t* surface){

	if(emulating){
		for(int i = 0; i < 32; i++){
			for(int j = 0; j < 64; j++){
				uint8_t colour = (display[i*64 + j]) ? 255 : 0;
				DrawRect(j*EMU_PIXEL_SIZE,i*EMU_PIXEL_SIZE,EMU_PIXEL_SIZE,EMU_PIXEL_SIZE,colour,colour,colour,surface);
			}
		}
		DrawString(str, 0, 0, 255, 0, 0, surface);
		
		DrawString(opcodeStringBuffer, 0, 13, 255, 0, 0, surface);
	} else {
		for(int i = 0; i < romCount; i++){
			if(romIndex == i){
				DrawRect(10,10 + 18*i, 492, 16, 255, 255, 255, surface);
				DrawString(romList[i], 10,10 + 18*i + 2, 0, 0, 0, surface);
			} else {
				DrawRect(10,10 + 18*i, 492, 16, 0, 0, 0, surface);
				DrawString(romList[i], 10,10 + 18*i + 2, 255, 255, 255, surface);
			}
		}
	}
}

void cycle(){
	if(instructionPointer >= 4096){
		strcpy(str, "Invalid memory location");
		return;
	}

	if(delayTimer){
		delayTimer -= 0.111;
	}

	uint16_t opcode = memory[instructionPointer] << 8 | memory[instructionPointer + 1];

	itoa(opcode, opcodeStringBuffer, 16);
	str = opcodeStringBuffer;

	switch(opcode & 0xF000){
		case 0x0000:
			if(opcode == 0x00E0){
				memset(display, 0, 64*32);
				drawFlag = true;
			} else if(opcode == 0x00EE){
				instructionPointer = stack[--stackPointer];
			} else {
				str = invalidOpcodeStr;
				return;
			}
			break;
		case 0x1000:
			instructionPointer = opcode & 0xFFF;
			return;
			break;
		case 0x2000:
			stack[stackPointer++] = instructionPointer;
			instructionPointer = opcode & 0xFFF;
			return;
			break;
		case 0x3000:
			if(v[(opcode >> 8) & 0xF] == (opcode & 0xFF)){
				instructionPointer += 2;
			}
			break;
		case 0x4000:
			if(v[(opcode >> 8) & 0xF] != (opcode & 0xFF)){
				instructionPointer += 2;
			}
			break;
		case 0x5000:
			if(v[(opcode >> 8) & 0xF] == v[(opcode >> 4) & 0xF]){
				instructionPointer += 2;
			}
			break;
		case 0x6000:
			v[(opcode >> 8) & 0xF] = opcode & 0xFF;
			break;
		case 0x7000:
			v[(opcode >> 8) & 0xF] += opcode & 0xFF;
			break;
		case 0x8000:
			switch (opcode & 0xF){
			case 0:
				v[(opcode >> 8) & 0xF] = v[(opcode >> 4) & 0xF];
				break;
			case 1:
				v[(opcode >> 8) & 0xF] |= v[(opcode >> 4) & 0xF];
				break;
			case 2:
				v[(opcode >> 8) & 0xF] &= v[(opcode >> 4) & 0xF];
				break;
			case 3:
				v[(opcode >> 8) & 0xF] ^= v[(opcode >> 4) & 0xF];
				break;
			case 4:
				v[(opcode >> 8) & 0xF] += v[(opcode >> 4) & 0xF];
				if((v[(opcode >> 8) & 0xF] + v[(opcode >> 4) & 0xF]) > 255) v[0xF] = 1;
				else v[0xF] = 0;
				break;
			case 5:
				if((v[(opcode >> 8) & 0xF] - v[(opcode >> 4) & 0xF]) < 0) v[0xF] = 1;
				else v[0xF] = 0;
				v[(opcode >> 8) & 0xF] = v[(opcode >> 4) & 0xF];
				break;
			case 6:
				v[0xF] = v[(opcode >> 8) & 0xF] & 0x1;
				v[(opcode >> 8) & 0xF] >>= 1;
				break;
			case 7:
				v[(opcode >> 8) & 0xF] = v[(opcode >> 4) & 0xF] - v[(opcode >> 8) & 0xF];
				break;
			case 0xE:
				v[0xF] = v[(opcode << 8) & 0xF];
				v[(opcode >> 8) & 0xF] <<= 1;
				break;
			default:
				str = invalidOpcodeStr;
				break;
			}
			break;
		case 0x9000:
			if(v[(opcode >> 8) & 0xF] != v[(opcode >> 4) & 0xF]) instructionPointer += 2;
			break;
		case 0xA000:
			indexRegister = opcode & 0xFFF;
			break;
		case 0xC000:
			v[(opcode >> 8) & 0xF] = rand() & (opcode & 0xFF);
			break;
		case 0xD000: {
			int x = v[(opcode >> 8) & 0xF];
            int y = v[(opcode >> 4) & 0xF];
            int height = opcode & 0x000F;
            int pixel;

            v[0xF] = 0;
            for (int yline = 0; yline < height; yline++)
            {
                pixel = memory[indexRegister + yline];
                for(int xline = 0; xline < 8; xline++)
                {
                    if((pixel & (0x80 >> xline)) != 0)
                    {
                        if(display[(x + xline + ((y + yline) * 64))] == 1)
                        {
                            v[0xF] = 1;
                        }
                        display[x + xline + ((y + yline) * 64)] ^= 1;
                    }
                }
            }
			drawFlag = true;
			break;
		}
		case 0xE000:
			switch (opcode & 0xFF)
			{
			case 0x9E:
				if(key[v[(opcode >> 8) & 0xF]])	instructionPointer+= 2;
				break;
			case 0xA1:
				if(!key[v[(opcode >> 8) & 0xF]]) instructionPointer+= 2;
				break;
			default:
				str = invalidOpcodeStr;
				drawFlag = true;
				break;
			}
			break;
		case 0xF000:
			switch (opcode & 0xFF)
			{
			case 0x07:
				v[(opcode >> 8) & 0xF] = delayTimer;
				break;
			case 0x0A: // Wait for key press
				//keyWait = true;
				//keyWaitRegister = (opcode >> 8) & 0xF;
				break;
			case 0x15:
				delayTimer = v[(opcode >> 8) & 0xF];
				break;
			case 0x18:
				delayTimer = v[(opcode >> 8) & 0xF];
				break;
			case 0x1E:
				indexRegister += v[(opcode >> 8) & 0xF];
				break;
			case 0x29:
				indexRegister = v[(opcode >> 8) & 0xF] * 0x5;
				break;
			case 0x33:
				memory[indexRegister]= v[(opcode >> 8) & 0xF] / 100;
                memory[indexRegister + 1] = (v[(opcode >> 8) & 0xF] / 10) % 10;
				memory[indexRegister + 2] = v[(opcode >> 8) & 0xF] % 10;
				break;
			case 0x55:
				memcpy(memory + indexRegister, v, (opcode >> 8) & 0xF);
				indexRegister += (opcode >> 8) & 0xF + 1;
				break;
			case 0x65:
				memcpy(v,memory + indexRegister,(opcode >> 8) & 0xF);
				indexRegister += (opcode >> 8) & 0xF + 1;
				break;
			default:
				str = invalidOpcodeStr;
				drawFlag = true;
				break;
			}
			break;
		default:
			str = invalidOpcodeStr;
			drawFlag = true;
			break;
	}
	instructionPointer += 2;
}

extern "C"
int main(char argc, char** argv){
	win_info_t windowInfo;

	windowInfo.width = 512;
	windowInfo.height = 256;
	windowInfo.x = 50;
	windowInfo.y = 50;
	windowInfo.flags = 0;

	Window* win = CreateWindow(&windowInfo);
	win->background = {0, 0, 0, 255};
	win->OnPaint = OnPaint;

	FILE* romFile = fopen("/tetris.c8", "r");
	size_t romSize = ftell(romFile);
	uint8_t* buffer = (uint8_t*)malloc(romSize);
	fread(buffer,romSize,1,romFile);

	memcpy(memory + 0x200, buffer, romSize);
	fclose(romFile);
	free(buffer);

	for(int i = 0; i < 80; i++){
		memory[i] = fontset[i];
	}

	display[100] = 1;

	for(;;){
		Wait();
		//memset(key,0,16);
		ipc_message_t msg;
		while(ReceiveMessage(&msg)){
			if(msg.id == WINDOW_EVENT_KEY && emulating){
				switch (msg.data)
				{
				case KEY_ESCAPE:
					emulating = false;
					//keyWait = false;
					break;
				case '1':
					key[1] = 1;
					currentKey = 1;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case '2':
					key[2] = 1;
					currentKey = 2;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case '3':
					key[3] = 1;
					currentKey = 3;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case '4':
					key[0xC] = 1;
					currentKey = 0xC;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'q':
					key[4] = 1;
					currentKey = 4;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'w':
					key[5] = 1;
					currentKey = 5;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'e':
					key[6] = 1;
					currentKey = 6;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'r':
					key[0xD] = 1;
					currentKey = 0xD;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'a':
					key[7] = 1;
					currentKey = 7;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 's':
					key[8] = 1;
					currentKey = 8;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'd':
					key[9] = 1;
					currentKey = 9;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'f':
					key[0xE] = 1;
					currentKey = 0xE;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'z':
					key[0xA] = 1;
					currentKey = 0xA;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'x':
					key[0] = 1;
					currentKey = 0;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'c':
					key[0xB] = 1;
					currentKey = 0xB;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				case 'v':
					key[0xF] = 1;
					currentKey = 0xF;
					keyWait = false;
					v[keyWaitRegister] = currentKey;
					break;
				}
			} else if(msg.id == WINDOW_EVENT_KEYRELEASED && emulating){
				//memset(key,0,16);
				switch (msg.data)
				{
				case '1':
					key[1] = 0;
					break;
				case '2':
					key[2] = 0;
					break;
				case '3':
					key[3] = 0;
					break;
				case '4':
					key[0xC] = 0;
					break;
				case 'q':
					key[4] = 0;
					break;
				case 'w':
					key[5] = 0;
					break;
				case 'e':
					key[6] = 0;
					break;
				case 'r':
					key[0xD] = 0;
					break;
				case 'a':
					key[7] = 0;
					break;
				case 's':
					key[8] = 0;
					break;
				case 'd':
					key[9] = 0;
					break;
				case 'f':
					key[0xE] = 0;
					break;
				case 'z':
					key[0xA] = 0;
					break;
				case 'x':
					key[0] = 0;
					break;
				case 'c':
					key[0xB] = 0;
					break;
				case 'v':
					key[0xF] = 0;
					break;
				}
			} else if (msg.id == WINDOW_EVENT_KEY && !emulating) {
				if(msg.data == KEY_ARROW_DOWN){
					if(++romIndex >= romCount) romIndex = 0;
				} else if (msg.data == KEY_ARROW_UP){
					if(--romIndex < 0) romIndex = romCount - 1;
				} else if (msg.data == KEY_ENTER){
					memset(display, 0, 2048);
					romFile = fopen(romList[romIndex], "r");
					romSize = ftell(romFile);
					buffer = (uint8_t*)malloc(romSize);
					fread(buffer,romSize,1,romFile);

					memset(memory + 0x200, 0, 4096 - 0x200);
					memcpy(memory + 0x200, buffer, romSize);
					free(buffer);
					fclose(romFile);

					instructionPointer = 0x200;
					indexRegister = 0;
					memset(v, 0, 16);

					emulating = true;
					keyWait = false;
				}
			}
		}

		if(emulating){
			if(!keyWait)
				cycle();
			if(str == invalidOpcodeStr){
				PaintWindow(win);
				for(;;);
			} 
			if(drawFlag){
				PaintWindow(win);
				drawFlag = false;
			}
		} else {
			PaintWindow(win);
		}
	}
}

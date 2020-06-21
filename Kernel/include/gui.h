#pragma once

#include <stdint.h>
#include <list.h>
#include <scheduler.h>

#define WINDOW_COUNT_MAX 65535

struct process;

typedef struct Vector2i{
    int x, y;
} vector2i_t; // Two dimensional integer vector

typedef struct Rect{
    vector2i_t pos;
    vector2i_t size;
} rect_t; // Rectangle

typedef struct Surface{
	int x, y, width, height, pitch; // Self-explanatory
	uint8_t depth; // Pixel depth
	uint8_t* buffer; // Start of the buffer
	uint8_t linePadding;
} surface_t;

typedef struct GUIContext{
	surface_t* surface;
	process* owner;
	uint32_t ownerPID;
} gui_context_t;

bool operator==(const Vector2i& l, const Vector2i& r);

typedef struct {
	uint16_t x = 0;
	uint16_t y = 0;

	uint16_t width = 0; // Width
	uint16_t height = 0; // Height

	uint32_t flags; // Window Flags

	char title[96]; // Title of window

	uint64_t primaryBufferKey;
	uint64_t secondaryBufferKey;
	uint8_t currentBuffer;

	uint64_t ownerPID;

	handle_t handle;
} __attribute__((packed)) win_info_t;

struct Desktop;

typedef struct Window{
	win_info_t info;

	Desktop* desktop;
} window_t;

typedef struct {
	uint16_t windowCount;
	uint16_t maxWindowCount = WINDOW_COUNT_MAX;
	uint8_t dirty;
	uint8_t reserved[3];
	handle_t windows[];
} __attribute__((packed)) window_list_t;

typedef struct Desktop{
	window_list_t* windows;

	uint64_t pid;

	volatile int lock = 0;
} __attribute__((packed)) desktop_t;

void SetDesktop(desktop_t* desktop);
desktop_t* GetDesktop();
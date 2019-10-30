/*
 * sdl_keymap.c
 *
 * Mappings from SDL keycode to local key codes.
 * Stolen from xkeymap.c
 *
 */

#include <SDL/SDL_keysym.h>
#include "input.h"

int keymap[][2] = {
	{ SDLK_LSHIFT, K_SHIFT },
	{ SDLK_RSHIFT, K_SHIFT },
	{ SDLK_LCTRL, K_CTRL },
	{ SDLK_RCTRL, K_CTRL },
	{ SDLK_LALT, K_ALT },
	{ SDLK_RALT, K_ALT },
	{ SDLK_LMETA, K_ALT },
	{ SDLK_RMETA, K_ALT },
	
	{ SDLK_UP, K_UP },
	{ SDLK_DOWN, K_DOWN },
	{ SDLK_RIGHT, K_RIGHT },
	{ SDLK_LEFT, K_LEFT },
	{ SDLK_RETURN, K_ENTER },
	{ SDLK_SPACE, K_SPACE },
	{ SDLK_TAB, K_TAB },
	{ SDLK_BACKSPACE, K_BS },
	{ SDLK_DELETE, K_DEL },    
	{ SDLK_INSERT, K_INS },
	{ SDLK_HOME, K_HOME },
	{ SDLK_END, K_END },
	{ SDLK_ESCAPE, K_ESC },
	{ SDLK_PAUSE, K_PAUSE },
	{ SDLK_BREAK, K_PAUSE },
	{ SDLK_CAPSLOCK, K_CAPS },
	{ SDLK_NUMLOCK, K_NUMLOCK },
	{ SDLK_SCROLLOCK, K_SCROLL },
	
	{ SDLK_MINUS, K_MINUS },
	{ SDLK_EQUALS, K_EQUALS },

	{ SDLK_LEFTBRACKET, '[' },
	{ SDLK_RIGHTBRACKET, ']' },
	{ SDLK_BACKSLASH, K_BSLASH },
	{ SDLK_BACKQUOTE, K_TILDE },
	{ SDLK_SEMICOLON, K_SEMI },
	{ SDLK_QUOTE, K_QUOTE },
	{ SDLK_QUOTEDBL, K_QUOTE },
	{ SDLK_COMMA, ',' },
	{ SDLK_PERIOD, '.' },
	{ SDLK_SLASH, '/' },
	
	{ SDLK_F1, K_F1 },
	{ SDLK_F2, K_F2 },
	{ SDLK_F3, K_F3 },
	{ SDLK_F4, K_F4 },
	{ SDLK_F5, K_F5 },
	{ SDLK_F6, K_F6 },
	{ SDLK_F7, K_F7 },
	{ SDLK_F8, K_F8 },
	{ SDLK_F9, K_F9 },
	{ SDLK_F10, K_F10 },
	{ SDLK_F11, K_F11 },
	{ SDLK_F12, K_F12 },
	
	{ SDLK_KP0, K_NUM0 },
	{ SDLK_KP1, K_NUM1 },
	{ SDLK_KP2, K_NUM2 },
	{ SDLK_KP3, K_NUM3 },
	{ SDLK_KP4, K_NUM4 },
	{ SDLK_KP5, K_NUM5 },
	{ SDLK_KP6, K_NUM6 },
	{ SDLK_KP7, K_NUM7 },
	{ SDLK_KP8, K_NUM8 },
	{ SDLK_KP9, K_NUM9 },
	{ SDLK_KP_PLUS, K_NUMPLUS },
	{ SDLK_KP_MINUS, K_NUMMINUS },
	{ SDLK_KP_MULTIPLY, K_NUMMUL },
	{ SDLK_KP_DIVIDE, K_NUMDIV },
	{ SDLK_KP_PERIOD, K_NUMDOT },
	{ SDLK_KP_ENTER, K_NUMENTER },

	{ 0, 0 }
};


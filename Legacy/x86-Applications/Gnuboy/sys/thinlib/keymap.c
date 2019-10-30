/*
 * pckeymap.c
 *
 * Mappings from IBM-PC scancodes to local key codes.
 */

#include "input.h"
#include "thinlib.h"

int keymap[][2] =
{
	{ THIN_KEY_ESC, K_ESC },
	{ THIN_KEY_1, '1' },
	{ THIN_KEY_2, '2' },
	{ THIN_KEY_3, '3' },
	{ THIN_KEY_4, '4' },
	{ THIN_KEY_5, '5' },
	{ THIN_KEY_6, '6' },
	{ THIN_KEY_7, '7' },
	{ THIN_KEY_8, '8' },
	{ THIN_KEY_9, '9' },
	{ THIN_KEY_0, '0' },
	{ THIN_KEY_MINUS, K_MINUS },
	{ THIN_KEY_EQUALS, K_EQUALS },
	{ THIN_KEY_BACKSPACE, K_BS },
	{ THIN_KEY_TAB, K_TAB },
	
	{ THIN_KEY_Q, 'q' },
	{ THIN_KEY_W, 'w' },
	{ THIN_KEY_E, 'e' },
	{ THIN_KEY_R, 'r' },
	{ THIN_KEY_T, 't' },
	{ THIN_KEY_Y, 'y' },
	{ THIN_KEY_U, 'u' },
	{ THIN_KEY_I, 'i' },
	{ THIN_KEY_O, 'o' },
	{ THIN_KEY_P, 'p' },
	
	{ THIN_KEY_OPEN_BRACE, '[' },
	{ THIN_KEY_CLOSE_BRACE, ']' },
	
	{ THIN_KEY_ENTER, K_ENTER },
	{ THIN_KEY_LEFT_CTRL, K_CTRL },
	
	{ THIN_KEY_A, 'a' },
	{ THIN_KEY_S, 's' },
	{ THIN_KEY_D, 'd' },
	{ THIN_KEY_F, 'f' },
	{ THIN_KEY_G, 'g' },
	{ THIN_KEY_H, 'h' },
	{ THIN_KEY_J, 'j' },
	{ THIN_KEY_K, 'k' },
	{ THIN_KEY_L, 'l' },
	
	{ THIN_KEY_SEMICOLON, K_SEMI },
	{ THIN_KEY_QUOTE, '\'' },
	{ THIN_KEY_TILDE, K_TILDE },
	{ THIN_KEY_LEFT_SHIFT, K_SHIFT },
	{ THIN_KEY_BACKSLASH, K_BSLASH },
	
	{ THIN_KEY_Z, 'z' },
	{ THIN_KEY_X, 'x' },
	{ THIN_KEY_C, 'c' },
	{ THIN_KEY_V, 'v' },
	{ THIN_KEY_B, 'b' },
	{ THIN_KEY_N, 'n' },
	{ THIN_KEY_M, 'm' },
	
	{ THIN_KEY_COMMA, ',' },
	{ THIN_KEY_PERIOD, '.' },
	{ THIN_KEY_SLASH, '/' },
	
	{ THIN_KEY_RIGHT_SHIFT, K_SHIFT },
	
	{ THIN_KEY_NUMPAD_MULT, K_NUMMUL },
	
	{ THIN_KEY_LEFT_ALT, K_ALT },
	{ THIN_KEY_SPACE, ' ' },
	{ THIN_KEY_CAPS_LOCK, K_CAPS },
	
	{ THIN_KEY_F1, K_F1 },
	{ THIN_KEY_F2, K_F2 },
	{ THIN_KEY_F3, K_F3 },
	{ THIN_KEY_F4, K_F4 },
	{ THIN_KEY_F5, K_F5 },
	{ THIN_KEY_F6, K_F6 },
	{ THIN_KEY_F7, K_F7 },
	{ THIN_KEY_F8, K_F8 },
	{ THIN_KEY_F9, K_F9 },
	{ THIN_KEY_F10, K_F10 },
	
	{ THIN_KEY_NUM_LOCK, K_NUMLOCK },
	{ THIN_KEY_SCROLL_LOCK, K_SCROLL },
	
	{ THIN_KEY_NUMPAD_7, K_NUM7 },
	{ THIN_KEY_NUMPAD_8, K_NUM8 },
	{ THIN_KEY_NUMPAD_9, K_NUM9 },
	{ THIN_KEY_NUMPAD_MINUS, K_NUMMINUS },
	{ THIN_KEY_NUMPAD_4, K_NUM4 },
	{ THIN_KEY_NUMPAD_5, K_NUM5 },
	{ THIN_KEY_NUMPAD_6, K_NUM6 },
	{ THIN_KEY_NUMPAD_PLUS, K_NUMPLUS },
	{ THIN_KEY_NUMPAD_1, K_NUM1 },
	{ THIN_KEY_NUMPAD_2, K_NUM2 },
	{ THIN_KEY_NUMPAD_3, K_NUM3 },
	{ THIN_KEY_NUMPAD_0, K_NUM0 },
	{ THIN_KEY_NUMPAD_DECIMAL, K_NUMDOT },
	
	{ THIN_KEY_F11, K_F11 },
	{ THIN_KEY_F12, K_F12 },
	
	{ THIN_KEY_NUMPAD_ENTER, K_NUMENTER },
	{ THIN_KEY_RIGHT_CTRL, K_CTRL },
	{ THIN_KEY_NUMPAD_DIV, K_NUMDIV },
	{ THIN_KEY_SYSRQ, K_SYSRQ },
	
	{ THIN_KEY_RIGHT_ALT, K_ALT },
	//{ THIN_KEY_PAUSE, K_PAUSE },
	
	{ THIN_KEY_HOME, K_HOME },
	{ THIN_KEY_UP, K_UP },
	{ THIN_KEY_PGUP, K_PRIOR },
	{ THIN_KEY_LEFT, K_LEFT },
	{ THIN_KEY_RIGHT, K_RIGHT },
	{ THIN_KEY_END, K_END },
	{ THIN_KEY_DOWN, K_DOWN },
	{ THIN_KEY_PGDN, K_NEXT },
	{ THIN_KEY_INSERT, K_INS },
	{ THIN_KEY_DELETE, K_DEL },

	{ 0, 0 }
};










/*
 * pckeymap.c
 *
 * Mappings from IBM-PC scancodes to local key codes.
 */

#include "input.h"

int keymap[][2] =
{
	{ 1, K_ESC },
	{ 2, '1' },
	{ 3, '2' },
	{ 4, '3' },
	{ 5, '4' },
	{ 6, '5' },
	{ 7, '6' },
	{ 8, '7' },
	{ 9, '8' },
	{ 10, '9' },
	{ 11, '0' },
	{ 12, K_MINUS },
	{ 13, K_EQUALS },
	{ 14, K_BS },
	{ 15, K_TAB },
	
	{ 16, 'q' },
	{ 17, 'w' },
	{ 18, 'e' },
	{ 19, 'r' },
	{ 20, 't' },
	{ 21, 'y' },
	{ 22, 'u' },
	{ 23, 'i' },
	{ 24, 'o' },
	{ 25, 'p' },
	
	{ 26, '[' },
	{ 27, ']' },
	
	{ 28, K_ENTER },
	{ 29, K_CTRL },
	
	{ 30, 'a' },
	{ 31, 's' },
	{ 32, 'd' },
	{ 33, 'f' },
	{ 34, 'g' },
	{ 35, 'h' },
	{ 36, 'j' },
	{ 37, 'k' },
	{ 38, 'l' },
	
	{ 39, K_SEMI },
	{ 40, '\'' },
	{ 41, K_TILDE },
	{ 42, K_SHIFT },
	{ 43, K_BSLASH },
	
	{ 44, 'z' },
	{ 45, 'x' },
	{ 46, 'c' },
	{ 47, 'v' },
	{ 48, 'b' },
	{ 49, 'n' },
	{ 50, 'm' },
	
	{ 51, ',' },
	{ 52, '.' },
	{ 53, '/' },
	
	{ 54, K_SHIFT },
	
	{ 55, K_NUMMUL },
	
	{ 56, K_ALT },
	{ 57, ' ' },
	{ 58, K_CAPS },
	
	{ 59, K_F1 },
	{ 60, K_F2 },
	{ 61, K_F3 },
	{ 62, K_F4 },
	{ 63, K_F5 },
	{ 64, K_F6 },
	{ 65, K_F7 },
	{ 66, K_F8 },
	{ 67, K_F9 },
	{ 68, K_F10 },
	
	{ 69, K_NUMLOCK },
	{ 70, K_SCROLL },
	
	{ 71, K_NUM7 },
	{ 72, K_NUM8 },
	{ 73, K_NUM9 },
	{ 74, K_NUMMINUS },
	{ 75, K_NUM4 },
	{ 76, K_NUM5 },
	{ 77, K_NUM6 },
	{ 78, K_NUMPLUS },
	{ 79, K_NUM1 },
	{ 80, K_NUM2 },
	{ 81, K_NUM3 },
	{ 82, K_NUM0 },
	{ 83, K_NUMDOT },
	
	{ 87, K_F11 },
	{ 88, K_F12 },
	
	{ 96, K_NUMENTER },
	{ 97, K_CTRL },
	{ 98, K_NUMDIV },
	{ 99, K_SYSRQ },
	
	{ 100, K_ALT },
	{ 101, K_PAUSE },
	{ 119, K_PAUSE },
	
	{ 102, K_HOME },
	{ 103, K_UP },
	{ 104, K_PRIOR },
	{ 105, K_LEFT },
	{ 106, K_RIGHT },
	{ 107, K_END },
	{ 108, K_DOWN },
	{ 109, K_NEXT },
	{ 110, K_INS },
	{ 111, K_DEL },

	{ 0, 0 }
};










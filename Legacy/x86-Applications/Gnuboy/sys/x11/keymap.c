/*
 * xkeymap.c
 *
 * Mappings from X keycode to local key codes.
 */

#include <X11/keysym.h>
#include "input.h"

int keymap[][2] =
{
	{ XK_Shift_L, K_SHIFT },
	{ XK_Shift_R, K_SHIFT },
	{ XK_Control_L, K_CTRL },
	{ XK_Control_R, K_CTRL },
	{ XK_Alt_L, K_ALT },
	{ XK_Alt_R, K_ALT },
	{ XK_Meta_L, K_ALT },
	{ XK_Meta_R, K_ALT },
	
	{ XK_Up, K_UP },
	{ XK_Down, K_DOWN },
	{ XK_Right, K_RIGHT },
	{ XK_Left, K_LEFT },
	{ XK_Return, K_ENTER },
	{ XK_Tab, K_TAB },
	{ XK_BackSpace, K_BS },
	{ XK_Delete, K_DEL },
	{ XK_Insert, K_INS },
	{ XK_Home, K_HOME },
	{ XK_End, K_END },
	{ XK_Prior, K_PRIOR },
	{ XK_Next, K_NEXT },
	{ XK_Escape, K_ESC },
	{ XK_Pause, K_PAUSE },
	{ XK_Caps_Lock, K_CAPS },
	{ XK_Num_Lock, K_NUMLOCK },
	{ XK_Scroll_Lock, K_SCROLL },
	
	{ XK_minus, K_MINUS },
	{ XK_equal, K_EQUALS },
	{ XK_asciitilde, K_TILDE },
	
	{ XK_F1, K_F1 },
	{ XK_F2, K_F2 },
	{ XK_F3, K_F3 },
	{ XK_F4, K_F4 },
	{ XK_F5, K_F5 },
	{ XK_F6, K_F6 },
	{ XK_F7, K_F7 },
	{ XK_F8, K_F8 },
	{ XK_F9, K_F9 },
	{ XK_F10, K_F10 },
	{ XK_F11, K_F11 },
	{ XK_F12, K_F12 },
	
	{ XK_KP_Insert, K_NUM0 },
	{ XK_KP_End, K_NUM1 },
	{ XK_KP_Down, K_NUM2 },
	{ XK_KP_Next, K_NUM3 },
	{ XK_KP_Left, K_NUM4 },
	{ XK_KP_Begin, K_NUM5 },
	{ XK_KP_Space, K_NUM5 },  /* FIXME - ??? */
	{ XK_KP_Right, K_NUM6 },
	{ XK_KP_Home, K_NUM7 },
	{ XK_KP_Up, K_NUM8 },
	{ XK_KP_Prior, K_NUM9 },
	{ XK_KP_0, K_NUM0 },
	{ XK_KP_1, K_NUM1 },
	{ XK_KP_2, K_NUM2 },
	{ XK_KP_3, K_NUM3 },
	{ XK_KP_4, K_NUM4 },
	{ XK_KP_5, K_NUM5 },
	{ XK_KP_6, K_NUM6 },
	{ XK_KP_7, K_NUM7 },
	{ XK_KP_8, K_NUM8 },
	{ XK_KP_9, K_NUM9 },
	{ XK_KP_Add, K_NUMPLUS },
	{ XK_KP_Subtract, K_NUMMINUS },
	{ XK_KP_Multiply, K_NUMMUL },
	{ XK_KP_Divide, K_NUMDIV },
	{ XK_KP_Separator, K_NUMDOT },
	{ XK_KP_Delete, K_NUMDOT },
	{ XK_KP_Enter, K_NUMENTER },

	{ 0, 0 }
};










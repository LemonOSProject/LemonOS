#include "input.h"

#include "main.h"

#include <lemon/keyboard.h>
#include <assert.h>

KeyboardState keyboardState;

int keymap_us[128] =
{
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
	KEY_CONTROL,			/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', '`',   KEY_SHIFT,		/* Left shift */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   0,				/* Right shift */
	'*',
	KEY_ALT,	/* Alt */
	' ',	/* Space bar */
	KEY_CAPS,	/* Caps lock */
	KEY_F1,	/* 59 - F1 key ... > */
	KEY_F2,   KEY_F3,   KEY_F4,   KEY_F5,   KEY_F6,   KEY_F7,   KEY_F8,   KEY_F9,
	KEY_F10,	/* < ... F10 */
	0,	/* 69 - Num lock*/
	0,	/* Scroll Lock */
	0,	/* Home key */
	KEY_ARROW_UP,	/* Up Arrow */
	0,	/* Page Up */
	'-',
	KEY_ARROW_LEFT,	/* Left Arrow */
	0,
	KEY_ARROW_RIGHT,	/* Right Arrow */
	'+',
	0,	/* 79 - End key*/
	KEY_ARROW_DOWN,	/* Down Arrow */
	0,	/* Page Down */
	0,	/* Insert Key */
	KEY_DELETE,	/* Delete Key */
	0,   0,   0,
	0,	/* F11 Key */
	0,	/* F12 Key */
	0,	/* All other keys are undefined */
	0,
	0,
	0,  /* 90 */
	KEY_GUI,  /* Left GUI key */
	KEY_GUI,  /* Right GUI key */
};

int GetKey(uint64_t code){
	code &= 0x7F;

    assert(code < 128);

    return keymap_us[code];
}

// Returns with 0 if a release, 1 if press
bool IsPress(uint64_t code){
	return !((code >> 7) & 1);
}

int HandleKeyEvent(ipc_message_t& msg, Window* active){
	uint64_t keycode = msg.data;
	int key = GetKey(keycode);
	bool isPressed = IsPress(keycode);

	switch (key)
	{
	case KEY_SHIFT:
		keyboardState.shift = isPressed;
		break;
	case KEY_CONTROL:
		keyboardState.control = isPressed;
		break;
	case KEY_ALT:
		keyboardState.alt = isPressed;
		break;
	case KEY_CAPS:
		if(isPressed) keyboardState.caps = !keyboardState.caps;
		break;
	case KEY_GUI:
		return ActionGUI;
		break;
	case KEY_F4:
		if(keyboardState.alt) return ActionCloseWindow;
		break;
	default:
		if(keyboardState.shift || keyboardState.caps) key = toupper(key);
		break;
	}

	if(!active) return ActionNone;

	ipc_message_t keyMsg;
	
	keyMsg.data2 = 0;
	if(keyboardState.shift) keyMsg.data2 |= KEY_STATE_SHIFT;
	if(keyboardState.control) keyMsg.data2 |= KEY_STATE_CONTROL;
	if(keyboardState.alt) keyMsg.data2 |= KEY_STATE_ALT;

	keyMsg.msg = isPressed ? WINDOW_EVENT_KEY : WINDOW_EVENT_KEYRELEASED;

	keyMsg.data = key;

	Lemon::SendMessage(active->info.ownerPID, keyMsg);

	return ActionNone;
}
#pragma once

#define KEY_TAB '\t'
#define KEY_BACKSPACE '\b'
#define KEY_ENTER '\n'
#define KEY_ESCAPE 27

#define KEY_F1 256
#define KEY_F2 257
#define KEY_F3 258
#define KEY_F4 259
#define KEY_F5 260
#define KEY_F6 261
#define KEY_F7 262
#define KEY_F8 263
#define KEY_F9 264
#define KEY_F10 265

#define KEY_ARROW_UP 266
#define KEY_ARROW_LEFT 267
#define KEY_ARROW_RIGHT 268
#define KEY_ARROW_DOWN 269

#define KEY_SHIFT 270
#define KEY_ALT 271
#define KEY_CONTROL 272
#define KEY_CAPS 273

#define KEY_DELETE 274
#define KEY_END 275
#define KEY_HOME 276
#define KEY_PGUP 277
#define KEY_PGDN 278
#define KEY_INS 279

#define KEY_GUI 280

#define KEY_STATE_CAPS 1
#define KEY_STATE_SHIFT 2
#define KEY_STATE_ALT 4
#define KEY_STATE_CONTROL 8

enum KeyModifier {
    KeyModifier_Control = 0x1,
    KeyModifier_Alt = 0x2,
    KeyModifier_Shift = 0x4,
};
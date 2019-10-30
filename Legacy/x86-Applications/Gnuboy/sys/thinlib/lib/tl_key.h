/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_key.h
**
** DOS keyboard handling defines / protos
**
** $Id: $
*/

#ifndef _TL_KEY_H_
#define _TL_KEY_H_

/* Keyboard scancodes */
#define  THIN_KEY_ESC            1
#define  THIN_KEY_1              2
#define  THIN_KEY_2              3
#define  THIN_KEY_3              4
#define  THIN_KEY_4              5
#define  THIN_KEY_5              6
#define  THIN_KEY_6              7
#define  THIN_KEY_7              8
#define  THIN_KEY_8              9
#define  THIN_KEY_9              10
#define  THIN_KEY_0              11
#define  THIN_KEY_MINUS          12
#define  THIN_KEY_EQUALS         13
#define  THIN_KEY_BACKSPACE      14
#define  THIN_KEY_TAB            15
#define  THIN_KEY_Q              16
#define  THIN_KEY_W              17
#define  THIN_KEY_E              18
#define  THIN_KEY_R              19
#define  THIN_KEY_T              20
#define  THIN_KEY_Y              21
#define  THIN_KEY_U              22
#define  THIN_KEY_I              23
#define  THIN_KEY_O              24
#define  THIN_KEY_P              25
#define  THIN_KEY_OPEN_BRACE     26
#define  THIN_KEY_CLOSE_BRACE    27
#define  THIN_KEY_ENTER          28
#define  THIN_KEY_LEFT_CTRL      29
#define  THIN_KEY_A              30
#define  THIN_KEY_S              31
#define  THIN_KEY_D              32
#define  THIN_KEY_F              33
#define  THIN_KEY_G              34
#define  THIN_KEY_H              35
#define  THIN_KEY_J              36
#define  THIN_KEY_K              37
#define  THIN_KEY_L              38
#define  THIN_KEY_SEMICOLON      39
#define  THIN_KEY_QUOTE          40
#define  THIN_KEY_TILDE          41
#define  THIN_KEY_LEFT_SHIFT     42
#define  THIN_KEY_BACKSLASH      43
#define  THIN_KEY_Z              44
#define  THIN_KEY_X              45
#define  THIN_KEY_C              46
#define  THIN_KEY_V              47
#define  THIN_KEY_B              48
#define  THIN_KEY_N              49
#define  THIN_KEY_M              50
#define  THIN_KEY_COMMA          51
#define  THIN_KEY_PERIOD         52
#define  THIN_KEY_SLASH          53
#define  THIN_KEY_RIGHT_SHIFT    54
#define  THIN_KEY_NUMPAD_MULT    55
#define  THIN_KEY_LEFT_ALT       56
#define  THIN_KEY_SPACE          57
#define  THIN_KEY_CAPS_LOCK      58
#define  THIN_KEY_F1             59
#define  THIN_KEY_F2             60
#define  THIN_KEY_F3             61
#define  THIN_KEY_F4             62
#define  THIN_KEY_F5             63
#define  THIN_KEY_F6             64
#define  THIN_KEY_F7             65
#define  THIN_KEY_F8             66
#define  THIN_KEY_F9             67
#define  THIN_KEY_F10            68
#define  THIN_KEY_NUM_LOCK       69
#define  THIN_KEY_SCROLL_LOCK    70

#define  THIN_KEY_F11            87
#define  THIN_KEY_F12            88

#define  THIN_KEY_NUMPAD_7       71
#define  THIN_KEY_NUMPAD_8       72
#define  THIN_KEY_NUMPAD_9       73
#define  THIN_KEY_NUMPAD_MINUS   74
#define  THIN_KEY_NUMPAD_4       75
#define  THIN_KEY_NUMPAD_5       76
#define  THIN_KEY_NUMPAD_6       77
#define  THIN_KEY_NUMPAD_PLUS    78
#define  THIN_KEY_NUMPAD_1       79
#define  THIN_KEY_NUMPAD_2       80
#define  THIN_KEY_NUMPAD_3       81
#define  THIN_KEY_NUMPAD_0       82
#define  THIN_KEY_NUMPAD_DECIMAL 83

/* TODO: investigate */
/*#define  THIN_KEY_PRINT_SCREEN   84*/

/* TODO: are these correct? 
** I don't have one of them new winders keyboards.
*/
#define  THIN_KEY_LEFT_WINDOWS   91
#define  THIN_KEY_RIGHT_WINDOWS  92
#define  THIN_KEY_MENU           93

/* extended keys */
#define  THIN_KEY_RIGHT_ALT      100
#define  THIN_KEY_RIGHT_CTRL     101
#define  THIN_KEY_NUMPAD_DIV     102
#define  THIN_KEY_NUMPAD_ENTER   103
#define  THIN_KEY_UP             104
#define  THIN_KEY_DOWN           105
#define  THIN_KEY_LEFT           106
#define  THIN_KEY_RIGHT          107
#define  THIN_KEY_INSERT         108
#define  THIN_KEY_HOME           109
#define  THIN_KEY_PGUP           110
#define  THIN_KEY_DELETE         111
#define  THIN_KEY_END            112
#define  THIN_KEY_PGDN           113

#define  THIN_KEY_BREAK          114
#define  THIN_KEY_SYSRQ          116

/* PAUSE generates a ^X sequence... bleh */
/*#define  THIN_KEY_PAUSE          115*/

#define  THIN_MAX_KEYS           128

extern int thin_key_init(void);
extern void thin_key_shutdown(void);
extern void thin_key_set_repeat(bool state);

#endif /* !_TL_KEY_H_ */

/*
** $Log: $
*/

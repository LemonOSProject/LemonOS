#pragma once

#define ANSI_ESC 27

#define ESC_SAVE_CURSOR '7'
#define ESC_RESTORE_CURSOR '8'

#define ANSI_DCS 'P' // Device Control String
#define ANSI_CSI '[' // Control 
#define ANSI_ST '\\' // String Terminator
#define ANSI_OSC ']' // Operating System Command
#define ANSI_SOS 'X' // Start of String
#define ANSI_PM  '^' // Privacy Message
#define ANSI_APC '_' // Application Program Command
#define ANSI_RIS 'c' // Reset to initial state

#define ANSI_CSI_CUU 'A' // Cursor Up
#define ANSI_CSI_CUD 'B' // Cursor Down
#define ANSI_CSI_CUF 'C' // Cursor Forward
#define ANSI_CSI_CUB 'D' // Cursor Back
#define ANSI_CSI_CNL 'E' // Cursor Next Line
#define ANSI_CSI_CPL 'F' // Cursor Previous Line
#define ANSI_CSI_CHA 'G' // Cursor Horizontal Absolute
#define ANSI_CSI_CUP 'H' // Cursor Position
#define ANSI_CSI_ED  'J' // Erase in Display
#define ANSI_CSI_EL  'K' // Erase in Line
#define ANSI_CSI_IL  'L' // Insert n Blank Lines
#define ANSI_CSI_DL  'M' // Delete n Lines
#define ANSI_CSI_SU  'S' // Scroll Up
#define ANSI_CSI_SD  'T' // Scroll Down

#define ANSI_CSI_SGR 'm' // Set Graphics Rendition
#define ANSI_CSI_SGR_RESET 0
#define ANSI_CSI_SGR_BOLD 1
#define ANSI_CSI_SGR_FAINT 2 // Faint/Halfbright
#define ANSI_CSI_SGR_ITALIC 3
#define ANSI_CSI_SGR_UNDERLINE 4
#define ANSI_CSI_SGR_SLOW_BLINK 5
#define ANSI_CSI_SGR_RAPID_BLINK 6
#define ANSI_CSI_SGR_REVERSE_VIDEO 7 // Swap foreground and background colours
#define ANSI_CSI_SGR_CONCEAL 8
#define ANSI_CSI_SGR_STRIKETHROUGH 9
#define ANSI_CSI_SGR_FONT_PRIMARY 10
#define ANSI_CSI_SGR_FONT_ALTERNATE 11 // Select font n - 10
#define ANSI_CSI_SGR_DOUBLE_UNDERLINE 21
#define ANSI_CSI_SGR_NORMAL_INTENSITY 22 // Disable blink and bold
#define ANSI_CSI_SGR_UNDERLINE_OFF 24
#define ANSI_CSI_SGR_BLINK_OFF 25
#define ANSI_CSI_SGR_REVERSE_VIDEO_OFF 27

#define ANSI_CSI_SGR_FG_BLACK 30 // Set foreground colour to black
#define ANSI_CSI_SGR_FG_RED 31
#define ANSI_CSI_SGR_FG_GREEN 32
#define ANSI_CSI_SGR_FG_BROWN 33
#define ANSI_CSI_SGR_FG_BLUE 34
#define ANSI_CSI_SGR_FG_MAGENTA 35
#define ANSI_CSI_SGR_FG_CYAN 36
#define ANSI_CSI_SGR_FG_WHITE 37
#define ANSI_CSI_SGR_FG 38 // Set foreground to 256/24-bit colour (;5;x (256 colour) or ;2;r;g;b (24bit colour))
#define ANSI_CSI_SGR_FG_DEFAULT 39

#define ANSI_CSI_SGR_BG_BLACK 40 // Set background colour to black
#define ANSI_CSI_SGR_BG_RED 41
#define ANSI_CSI_SGR_BG_GREEN 42
#define ANSI_CSI_SGR_BG_BROWN 43
#define ANSI_CSI_SGR_BG_BLUE 44
#define ANSI_CSI_SGR_BG_MAGENTA 45
#define ANSI_CSI_SGR_BG_CYAN 46
#define ANSI_CSI_SGR_BG_WHITE 47
#define ANSI_CSI_SGR_BG 48 // Set background to 256/24-bit colour
#define ANSI_CSI_SGR_BG_DEFAULT 49

#define ANSI_CSI_SGR_FG_BLACK_BRIGHT 90 // Set foreground colour to black
#define ANSI_CSI_SGR_FG_RED_BRIGHT 91
#define ANSI_CSI_SGR_FG_GREEN_BRIGHT 92
#define ANSI_CSI_SGR_FG_BROWN_BRIGHT 93
#define ANSI_CSI_SGR_FG_BLUE_BRIGHT 94
#define ANSI_CSI_SGR_FG_MAGENTA_BRIGHT 95
#define ANSI_CSI_SGR_FG_CYAN_BRIGHT 96
#define ANSI_CSI_SGR_FG_WHITE_BRIGHT 97

#define ANSI_CSI_SGR_BG_BLACK_BRIGHT 100 // Set background colour to black
#define ANSI_CSI_SGR_BG_RED_BRIGHT 101
#define ANSI_CSI_SGR_BG_GREEN_BRIGHT 102
#define ANSI_CSI_SGR_BG_BROWN_BRIGHT 103
#define ANSI_CSI_SGR_BG_BLUE_BRIGHT 104
#define ANSI_CSI_SGR_BG_MAGENTA_BRIGHT 105
#define ANSI_CSI_SGR_BG_CYAN_BRIGHT 106
#define ANSI_CSI_SGR_BG_WHITE_BRIGHT 107
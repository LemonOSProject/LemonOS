/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */
// from https://gitlab.com/qookei/quack

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "libc.h"

#define FMT_PUT(dst, len, c) {\
				if(!(len)) goto end; \
				*(dst)++ = (c); \
				len--; \
			}

static int isdigit(char c) {
	return c >= '0' && c <= '9';
}

static const char *digits_upper = "0123456789ABCDEF";
static const char *digits_lower = "0123456789abcdef";

static char *num_fmt(uint64_t i, int base, int padding, char pad_with, int handle_signed, int upper, int len) {
	int neg = (signed)i < 0 && handle_signed;

	if (neg)
		i = (unsigned)(-((signed)i));

	static char buf[50];
	char *ptr = buf + 49;
	*ptr = '\0';

	const char *digits = upper ? digits_upper : digits_lower;

	do {
		*--ptr = digits[i % base];
		if (padding)
			padding--;
		if (len > 0)
			len--;
	} while ((i /= base) != 0 && (len == -1 || len));

	while (padding) {
		*--ptr = pad_with;
		padding--;
	}

	if (neg)
		*--ptr = '-';

	return ptr;
}

void lai_vsnprintf(char *buf, size_t len, const char *fmt, va_list arg) {
	uint64_t i;
	char *s;

	while(*fmt && len) {
		if (*fmt != '%') {
			*buf++ = *fmt;
			fmt++;
			continue;
		}

		fmt++;
		int padding = 0;
		char pad_with = ' ';
		int wide = 0, upper = 0;

		if (*fmt == '0') {
			pad_with = '0';
			fmt++;
		}

		while (isdigit(*fmt)) {
			padding *= 10;			// noop on first iter
			padding += *fmt++ - '0';
		}

		while (*fmt == 'l') {
			wide = 1;
			fmt++;
		}

		upper = *fmt == 'X' || *fmt == 'P';

		switch (*fmt) {
			case 'c': {
				i = va_arg(arg, int);
				FMT_PUT(buf, len, i)
				break;
			}

			case 'd': {
				if (wide)
					i = va_arg(arg, long int);
				else
					i = va_arg(arg, int);

				char *c = num_fmt(i, 10, padding, pad_with, 1, 0, -1);
				while (*c) {
					FMT_PUT(buf, len, *c);
					c++;
				}
				break;
			}

			case 'u': {
				if (wide)
					i = va_arg(arg, long int);
				else
					i = va_arg(arg, int);

				char *c = num_fmt(i, 10, padding, pad_with, 0, 0, -1);
				while (*c) {
					FMT_PUT(buf, len, *c);
					c++;
				}
				break;
			}

			case 'o': {
				if (wide)
					i = va_arg(arg, long int);
				else
					i = va_arg(arg, int);

				char *c = num_fmt(i, 8, padding, pad_with, 0, 0, -1);
				while (*c) {
					FMT_PUT(buf, len, *c);
					c++;
				}
				break;
			}

			case 'X':
			case 'x': {
				if (wide)
					i = va_arg(arg, long int);
				else
					i = va_arg(arg, int);

				char *c = num_fmt(i, 16, padding, pad_with, 0, upper, wide ? 16 : 8);
				while (*c) {
					FMT_PUT(buf, len, *c);
					c++;
				}
				break;
			}

			case 'P':
			case 'p': {
				i = (uint64_t)(va_arg(arg, void *));

				char *c = num_fmt(i, 16, padding, pad_with, 0, upper, 16);
				while (*c) {
					FMT_PUT(buf, len, *c);
					c++;
				}
				break;
			}

			case 's': {
				s = va_arg(arg, char *);
				while (*s) {
					FMT_PUT(buf, len, *s);
					s++;
				}
				break;
			}

			case '%': {
				FMT_PUT(buf, len, '%');
				break;
			}
		}

		fmt++;
	}

end:
	if (!len) {
		*buf++ = '.';	// requires extra reserved space
		*buf++ = '.';
		*buf++ = '.';
	}

	*buf++ = '\0';
}


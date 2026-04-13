#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stddef.h>

void keyboard_init(void);

/* Read a character (blocking). Returns 0 for non-printable keys. */
char keyboard_getchar(void);

/* Read a line (blocking), echoes input; returns number of characters read
   (not including the terminating '\0'). */
size_t keyboard_readline(char *buf, size_t bufsz);

#endif /* KEYBOARD_H */

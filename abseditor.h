#pragma once
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#define ABS_TAB_STOP 8
#define C_KEY(k) (( k ) & 0x1f)
#define ABS_VERSION "0.0.1"
// editor key (non insertion keys)
enum editor_key {
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};
// row in the editor
typedef struct erow {
	int size;
	int rsize;
	char *chars;
	char *render;
} erow;

//editor @runtime
struct editor_config {
	int cx, cy;
	int rx;
	int rowoff;
	int coloff;
	int screenrows;
	int screencols;
	int numrows;
	erow *row;
	int dirty;
	char *filename;
	char statusmsg[80];
	time_t statusmsg_time;
	struct termios _term;
};

// to i buffer
struct abuf {
	char *b;
	int len;
};

struct termios _term;
struct editor_config E;
#define ABUF_INIT { NULL, 0 }
#define U_QUIT_TIMES 3;
void set_status(const char *fmt, ... );
void refresh_screen();
char *editor_prompt(char *prompt , void (*callback)( char *, int));

void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

void disable_raw() {
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E._term) == -1) die("disable_raw tcsetattr");
}

void enable_raw() {
	if(tcgetattr(STDIN_FILENO, &E._term ) == -1) die("enable_raw tcgetattr");
	atexit(disable_raw);
	struct termios raw = E._term;
	tcgetattr( STDIN_FILENO, &raw );
	raw.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("enable_raw tcsetattr");
}

void ab_append(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab-> b, ab -> len + len);
	if(new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab -> b = new;
	ab -> len += len;
}

void ab_free(struct abuf *ab) {
	free(ab -> b);
}

void free_row(erow *row) {
	free(row -> render);
	free(row -> chars);
}

void del_row(int at) {
	if( at < 0 || at >= E.numrows ) return;
	free_row(&E.row[at]);
	memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
	E.numrows--;
	E.dirty++;
}
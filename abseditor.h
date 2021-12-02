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
#define CTRL_KEY( k ) ( ( k ) & 0x1f )
#define ABS_VERSION "0.0.1"

enum editorKey {
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

typedef struct erow {
	int size;
	int rsize;
	char *chars;
	char *render;
} erow;



struct editorConfig {
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
	struct termios orig_termios;
};

struct termios orig_termios;

struct editorConfig E;

#define ABUF_INIT { NULL, 0 }
#define KILO_QUIT_TIMES 3;
void editorSetStatusMessage( const char *fmt, ... );
void editorRefreshScreen();
char *editorPrompt( char *prompt );




void die( const char *s ) {
	write( STDOUT_FILENO, "\x1b[2J", 4 );
	write( STDOUT_FILENO, "\x1b[H", 3 );
	perror( s );
	exit( 1 );
}

void disableRawMode() {
	if( tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios ) == -1 ) die( "tcsetattr" );
}


void enableRawMode() {
	if( tcgetattr(STDIN_FILENO, &E.orig_termios ) == -1 ) die( "tcgetattr" ); 
	atexit( disableRawMode );
	struct termios raw = E.orig_termios;
	tcgetattr( STDIN_FILENO, &raw );
	raw.c_iflag &= ~( BRKINT | ICRNL | ISTRIP | IXON );
	raw.c_oflag &= ~( OPOST );
	raw.c_cflag |= ( CS8 );
	raw.c_lflag &= ~( ECHO | ICANON | IEXTEN | ISIG );
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	if( tcsetattr( STDIN_FILENO, TCSAFLUSH, &raw ) == -1 ) die( "tcsetattr" );  
}
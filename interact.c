#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY( k ) ( ( k ) & 0x1f )

struct termios orig_termios;

struct editorConfig {
	struct termios orig_termios;
};

struct editorConfig E;

void die( const char *s ) {
	write( STDOUT_FILENO, "\x1b[2J", 4 );
	write( STDOUT_FILENO, "\x1b[H", 3 );
	perror( s );
	exit( 1 );
}

void disableRawMode() {
	if( tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios ) == -1 ) die( "tcsetattr" );

	//tcsetattr( STDIN_FILENO, TCSAFLUSH, &orig_termios );
}


void enableRawMode() {
	//tcgetattr( STDIN_FILENO, &orig_termios );
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

char editorReadKey() {
	int nread;
	char c;
	while( (nread = read( STDIN_FILENO, &c, 1 ) ) != 1 ) {
		if( nread == -1 && errno != EAGAIN ) die( "read" );
	}
	return c;
}

void editorDrawRows() {
	int y;
	for( y = 0; y < 24; y++ ) {
		write( STDOUT_FILENO, "~\r\n", 3 );
	}
}

void editorRefreshScreen() {
	write( STDOUT_FILENO, "\x1b[2J", 4 );
	write( STDOUT_FILENO, "\x1b[H", 3 );
	editorDrawRows();
	write( STDOUT_FILENO, "\x1b[H", 3 );
}
void editorProcessKeypress() {
	char c = editorReadKey();
	switch( c ) {
		case CTRL_KEY( 'q' ):
			write( STDOUT_FILENO, "\x1b[2J", 4 );
			write( STDOUT_FILENO, "\x1b[H", 3 );
			exit( 0 );
			break;
	}
}
int main() {
	enableRawMode();
	char c;

	while(1) {
		editorRefreshScreen();		
		editorProcessKeypress();

		//char c = '\0';

		//if( read( STDIN_FILENO, &c, 1 ) == -1 && errno != EAGAIN ) die( "read" );
		//if( iscntrl(c) ) {
		//	printf( "%d\r\n", c );
//						                }
		//else {
		//	printf( "%d ('%c')\r\n", c, c );
		//}
		//if( c == CTRL_KEY( 'q' ) ) break;
	}
	return 0;
}
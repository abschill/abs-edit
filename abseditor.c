
#include "abseditor.h"

int read_key() {
	int nread;
	char c;
	while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if(nread == -1 && errno != EAGAIN) die("read_key");
	}

	if(c == '\x1b') {
		char seq[3];
		if(read( STDIN_FILENO, &seq[0], 1 ) != 1) return '\x1b';
		if(read( STDIN_FILENO, &seq[1], 1 ) != 1) return '\x1b';

		if(seq[0] == '[') {
			if(seq[1] >= '0' && seq[1] <= '9') {
				if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if(seq[2] == '~' ) {
					switch(seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
				else {
					switch(seq[1]) {
						case 'A': return ARROW_UP;
						case 'B': return ARROW_DOWN;
						case 'C': return ARROW_RIGHT;
						case 'D': return ARROW_LEFT;
						case 'H': return HOME_KEY;
						case 'F': return END_KEY;
					}
				}
			}
			else if(seq[0] == 'O') {
				switch(seq[1]) {
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
			return '\x1b';
		}
	}
	else {
		return c;
	}
}

int get_cursor_pos(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while(i < sizeof(buf) - 1) {
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if(buf[i] == 'R') break;
		i++;
	}

	buf[i] = '\0';
	if(buf[0] != '\x1b' || buf[1] != '[') return -1;
	if(sscanf( &buf[2], "%d;%d", rows, cols) != 2) return -1;
	return 0;
}


int get_win_size(int *rows, int *cols) {
	struct winsize ws;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws ) == -1 || ws.ws_col == 0) {
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return get_cursor_pos(rows, cols);
	}
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

int r_cx_to_rx(erow *row, int cx) {
	int rx = 0;
	int j;

	for(j = 0; j < cx; j++) {
		if(row -> chars[j] == '\t') {
			rx += (TAB_STOP - 1) - (rx % TAB_STOP);
		}
		rx++;
	}
	return rx;
}

int r_rx_to_cx(erow *row, int rx) {
	int cur_rx = 0;
	int cx;
	for(cx = 0; cx < row -> size; cx++) {
		if(row -> chars[cx] == '\t') {
			cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
		}
		cur_rx++;
		if(cur_rx > rx) return cx;
	}
	return cx;
}


void update_erow(erow *row) {
	int tabs = 0;
	int j;

	for(j = 0; j < row -> size; j++) {
		if(row -> chars[j] == '\t') tabs++;
	}

	free( row -> render);

	row -> render = malloc(row -> size + tabs * (TAB_STOP - 1) + 1);

	int idx = 0;
	for(j = 0; j < row -> size; j++) {
		if(row -> chars[j] == '\t') {
			row -> render[idx++] = ' ';
			while(idx % TAB_STOP != 0) row -> render[idx++] = ' ';
		}
		else {
			row -> render[idx++] = row -> chars[j];
		}
	}
	row -> render[idx] = '\0';
	row -> rsize = idx;
}

void insert_row(int at, char *s, size_t len) {
	if(at < 0 || at > E.numrows) return;
	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
	memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy( E.row[at].chars, s, len );
	E.row[at].chars[len] = '\0';
	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	update_erow(&E.row[at]);
	E.numrows++;
	E.dirty++;
}

void r_insert_char(erow * row, int at, int c) {
	if(at < 0 || at >= row -> size) at = row -> size;
	row -> chars = realloc(row -> chars, row -> size +2);
	memmove(&row -> chars[at + 1], &row->chars[at], row->size - at + 1);
	row -> size++;
	row -> chars[at] = c;
	update_erow(row);
	E.dirty++;
}

void r_append_string(erow *row, char *s, size_t len) {
	row -> chars = realloc(row -> chars, row -> size + len + 1);
	memcpy(&row -> chars[row -> size ], s, len);
	row -> size += len;
	row -> chars[row -> size] = '\0';
	update_erow(row);
	E.dirty++;
}

void r_del_char(erow *row, int at) {
	if(at < 0 || at >= row -> size) return;
	memmove(&row -> chars[at], &row -> chars[at + 1], row -> size - at);
	row -> size--;
	update_erow(row);
	E.dirty++;
}

void insert_char(int c) {
	if(E.cy == E.numrows) {
		insert_row(E.numrows, "", 0);
	}
	r_insert_char(&E.row[E.cy], E.cx, c);
	E.cx++;
}

void add_ln() {
	if(E.cx == 0) {
		insert_row(E.cy, "", 0);
	}
	else {
		erow *row = &E.row[E.cy];
		insert_row(E.cy + 1, &row -> chars[E.cx], row -> size - E.cx);
		row = &E.row[E.cy];
		row -> size = E.cx;
		row -> chars[row -> size] = '\0';
		update_erow(row);
	}
	E.cy++;
	E.cx = 0;
}

void del_char() {
	if (E.cy == E.numrows) return;
	if (E.cx == 0 && E.cy == 0) return;

	erow * row = &E.row[E.cy];
	if(E.cx > 0) {
		r_del_char(row, E.cx - 1);
		E.cx--;
	}
	else {
		E.cx = E.row[E.cy - 1].size;
		r_append_string(&E.row[E.cy - 1], row -> chars, row -> size);
		del_row(E.cy);
		E.cy--;
	}
}

char *rs_toString(int *buflen) {
	int totlen = 0;
	int j;
	for(j = 0; j < E.numrows; j++) {
		totlen += E.row[j].size + 1;
		*buflen = totlen;
	}
	char *buf = malloc(totlen);
	char *p = buf;
	for(j = 0; j < E.numrows; j++) {
		memcpy(p, E.row[j].chars, E.row[j].size);
		p += E.row[j].size;
		*p = '\n';
		p++;
	}
	return buf;
}


void open_editor(char *filename) {
	free(E.filename);
	E.filename = strdup(filename);
	FILE *fp = fopen(filename, "r");
	if (!fp) die("open_file fp err");
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp ) ) != -1) {
		while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r' ))
			linelen--;
			insert_row(E.numrows, line, linelen);
	}
	free(line);
	fclose(fp);
	E.dirty = 0;
}

void editor_save() {
	if(E.filename == NULL) {
		E.filename = editor_prompt( "Save as: %s (esc to cancel)", NULL);
		if(E.filename == NULL) {
			set_status( "Save Aborted" );
			return;
		}
	}
	int len;
	char *buf = rs_toString(&len);

	int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
	if(fd != -1) {
		if(ftruncate(fd, len) != -1) {
			if( write(fd, buf, len) == len) {
				close(fd);
				free(buf);
				E.dirty = 0;
				set_status("%d bytes written to disk", len);
				return;
			}
		}
		close(fd);
	}
	free(buf);
	set_status("Can't Save, error: %s", strerror(errno));
}

void find_cb(char *query, int key) {
	static int last_match = -1;
	static int direction = 1;

	if(key == '\r' || key == '\x1b') {
		last_match = -1;
		direction = 1;
		return;
	}
	else if(key == ARROW_RIGHT || key == ARROW_DOWN) {
		direction = 1;
	}
	else if(key == ARROW_LEFT || key == ARROW_UP) {
		direction = -1;
	}
	else {
		last_match = -1;
		direction = 1;
	}

	if(last_match == -1) direction = 1;
	int current = last_match;

	int i;
	for(i = 0; i < E.numrows; i++) {
		current += direction;
		if(current == -1) current = E.numrows - 1;
		else if (current == E.numrows) current = 0;

		erow *row = &E.row[current];
		char *match = strstr(row -> render, query);
		if(match) {
			last_match = current;
			E.cy = current;
			E.cx = r_rx_to_cx(row, match - row -> render);
			E.rowoff = E.numrows;
			break;
		}
	}
}

void editor_find() {
	int saved_cx = E.cx;
	int saved_cy = E.cy;
	int saved_coloff = E.coloff;
	int saved_rowoff = E.rowoff;
	char *query = editor_prompt("Search: %s (arrows/enter/esc)", find_cb);
	if(query) {
		free(query);
	}
	else {
		E.cx = saved_cx;
		E.cy = saved_cy;
		E.coloff = saved_coloff;
		E.rowoff = saved_rowoff;
	}
}

void scroll() {
	E.rx = 0;
	if(E.cy < E.numrows) {
		E.rx = r_cx_to_rx( &E.row[E.cy], E.cx );
	}
	if (E.cy < E.rowoff) {
		E.rowoff = E.cy;
	}
	if(E.cy >= E.rowoff + E.screenrows) {
		E.rowoff = E.cy - E.screenrows + 1;
	}
	if(E.rx < E.coloff) {
		E.coloff = E.rx;
	}
	if(E.rx >= E.coloff + E.screencols) {
		E.coloff = E.rx - E.screencols + 1;
	}
}

void draw_rows(struct abuf *ab) {
	int y;
	for(y = 0; y < E.screenrows; y++) {
		int filerow = y + E.rowoff;
		if(filerow >= E.numrows) {
			if( E.numrows == 0 && y == E.screenrows / 3 ) {
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome), "Abs Editor -- version %s", ABSEDIT_VERSION);
				if(welcomelen > E.screencols) welcomelen = E.screencols;
				int padding = (E.screencols - welcomelen) / 2;
				if(padding) {
					ab_append(ab,  "~", 1);
					padding--;
				}
				while(padding --) ab_append(ab, " ", 1);
				ab_append(ab, welcome, welcomelen);
			}
			else {
				ab_append(ab, "~", 1);
			}

		} else {
			int len = E.row[filerow].size - E.coloff;
			if(len < 0) len = 0;
			if (len > E.screencols) len = E.screencols;
			ab_append(ab, &E.row[filerow].render[E.coloff], len);
		}
		ab_append(ab, "\x1b[K", 3);
		ab_append(ab, "\r\n", 2);
	}
}

void draw_statusbar(struct abuf *ab) {
	ab_append(ab, "\x1b[7m", 4);
	char status[80], rstatus[80];
	int len = snprintf(status, sizeof(status), "%.20s - %d lines, %s",
	E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
	if(len > E.screencols) len = E.screencols;
	ab_append( ab, status, len);
	while(len < E.screencols) {
		if(E.screencols - len == rlen) {
			ab_append(ab, rstatus, rlen);
			break;
		}
		else {
			ab_append(ab, " ", 1);
			len++;
		}
	}
	ab_append(ab, "\x1b[m", 3);
	ab_append(ab, "\r\n", 2);

}

void draw_msg_bar(struct abuf *ab) {
	ab_append( ab, "\x1b[K", 3);
	int msglen = strlen(E.statusmsg);
	if (msglen > E.screencols) msglen = E.screencols;
	if(msglen && time( NULL) - E.statusmsg_time < 5) ab_append(ab, E.statusmsg, msglen);
}


void refresh_screen() {
	scroll();
	struct abuf ab = ABUF_INIT;
	ab_append(&ab, "\x1b[?25l", 6);
	ab_append(&ab, "\x1b[H", 3);
	draw_rows(&ab);
	draw_statusbar(&ab);
	draw_msg_bar(&ab);
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff ) + 1, (E.rx - E.coloff) + 1);
	ab_append(&ab, buf, strlen(buf));
	ab_append(&ab, "\x1b[?25h", 6);
	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}

void set_status(const char *fmt, ... ) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap );
	va_end(ap);
	E.statusmsg_time = time( NULL );
}

char *editor_prompt( char *prompt, void( *callback )( char *, int ) ) {
	size_t bufsize = 128;
	char *buf = malloc( bufsize );
	size_t buflen = 0;
	buf[0] = '\0';

	while( 1 ) {
		set_status( prompt, buf );
		refresh_screen();
		int c = read_key();
		if( c == DEL_KEY || c == C_KEY( 'h' ) || c == BACKSPACE ) {
			if( buflen != 0 ) buf[--buflen] = '\0';
		}
		else if( c == '\x1b' ) {
			set_status( "" );
			if(callback ) callback( buf, c );
			free( buf );
			return NULL;
		}
		else if(c == '\r') {
			if(buflen != 0) {
				set_status("");
				if( callback ) callback( buf, c );
				return buf;
			}
		}
		else if(!iscntrl(c) && c < 128) {
			if(buflen == bufsize - 1) {
				bufsize *= 2;
				buf = realloc(buf, bufsize);
			}
			buf[buflen++] = c;
			buf[buflen] = '\0';
		}

		if(callback) callback(buf, c);
	}
}

void move_cursor(int key) {
	erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	switch(key) {
		case ARROW_LEFT:
			if(E.cx != 0) {
				E.cx--;
			}
			else if ( E.cy > 0) {
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_RIGHT:
			if(row && E.cx < row -> size) {
				E.cx++;
			}
			else if( row && E.cx == row -> size) {
				E.cy++;
				E.cx = 0;
			}
			break;
		case ARROW_UP:
			if(E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if(E.cy < E.numrows) {
				E.cy++;
			}
			break;
	}
	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row ? row -> size : 0;
	if(E.cx > rowlen) {
		E.cx = rowlen;
	}
}

void process_key() {
	static int quit_times = U_QUIT_TIMES;

	int c = read_key();
	switch(c) {
		case '\r':
			add_ln();
			break;
		case C_KEY('q'):
			if(E.dirty && quit_times > 0) {
				set_status("Warning: File has Unsaved changes, quit 2 more times to confirm", quit_times);
				quit_times--;
				return;
			}
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case C_KEY('s'):
			editor_save();
			break;

		case HOME_KEY:
			E.cx = 0;
			break;

		case END_KEY:
		if(E.cy < E.numrows)
			E.cx = E.row[E.cy].size;
			break;

		case C_KEY('f'):
			editor_find();
			break;

		case BACKSPACE:
		case C_KEY('h'):
		case DEL_KEY:
			if( c == DEL_KEY ) move_cursor(ARROW_RIGHT);
			del_char();
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				if( c == PAGE_UP ) {
					E.cy = E.rowoff;
				}
				else if( c == PAGE_DOWN ) {
					E.cy = E.rowoff + E.screenrows - 1;
					if( E.cy > E.numrows ) E.cy = E.numrows;
				}
				int times = E.screenrows;
				while(times--) {
					move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
				}
			}
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			move_cursor(c);
			break;

		case C_KEY('l'):
		case '\x1b':
			break;
		default:
			insert_char(c);
			break;
	}
	quit_times = U_QUIT_TIMES;
}

void editor() {
	E.cx = 0;
	E.cy = 0;
	E.rx = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row = NULL;
	E.dirty = 0;
	E.filename = NULL;
	E.statusmsg[0] = '\0';
	E.statusmsg_time = 0;
	if(get_win_size( &E.screenrows, &E.screencols ) == -1) die("get_win_size");
	E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
	enable_raw();
	editor();
	if(argc >= 2) {
		open_editor(argv[1]);
	}

	set_status( "Controls: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = Find");

	while(1) {
		refresh_screen();
		process_key();
	}
	return 0;
}

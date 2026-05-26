#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <termios.h>
#include <sys/ioctl.h>
#include "unistd.h"

//DEFINES
#define CTRL_KEY(k) ((k) & 0x1f)
#define VERSION "1.0"

enum editorKey
{
	ARROW_UP = 1000,

	ARROW_DOWN,

	ARROW_LEFT,

	ARROW_RIGHT,

	PAGE_UP, 

	PAGE_DOWN
}; 

//DATA
struct editorConfig
{
	int cx, cy; 
	int screenrows; 
	int screencolumns; 
	struct termios originTerm; 
}; 

struct editorConfig E; 

//TERMINAL 
void die(const char* msg)
{
	write(STDOUT_FILENO, "\x1b[2J", 4); 
	write(STDOUT_FILENO, "\x1b[H", 3); 
	perror(msg); 
	exit(1); 
}

void disableRawMode()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.originTerm) == -1)
	{
		die("tcsetattr"); 
	}
}

void enableRawMode()
{
	if(tcgetattr(STDIN_FILENO, &E.originTerm) == -1)
	{
		die("tcgetattr"); 
	}

	atexit(disableRawMode); 

	struct termios term = E.originTerm; 

	term.c_oflag &= ~(OPOST); 
	term.c_cflag |= CS8;
	term.c_iflag &= ~(IXON | ICRNL); 
	term.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); 
	term.c_cc[VMIN] = 0;
	term.c_cc[VTIME] = 1;

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == -1)
	{
		die("tcsetattr"); 
	}
}

int editorReadKey()
{
	char c; 
	int nread; 

	while ((nread = read(STDIN_FILENO, &c, 1) != 1))
	{
		if (nread == -1 && errno != EAGAIN) 
		{
			die("readKey:"); 
		}
	}

	if (c == '\x1b') 
	{
		char seq[3];
		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') 
		{
			if (seq[1] >= '0' && seq[1] <= '9') 
			{
				if (read(STDIN_FILENO, &seq[2], 1)) return '\x1b';

				if (seq[2] == '~') 
				{
					switch (seq[1]) 
					{
						case '5': return PAGE_UP; 
						case '6': return PAGE_DOWN; 
					}
				}
			} 
			else 
			{
				switch (seq[1]) 
				{
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
				}
			}
		}

		return '\x1b';
	} 

	return c;
}


int getCursorPosition(int* rows, int* columns)
{
	char buf[32];
	write(STDOUT_FILENO, "\x1b[6n", 4); 
	char c; 

	printf("\r\n");
	int i = 0; 

	while (i < sizeof(buf) - 1) 
	{
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') 
		{
			break;
		}
		i++;
	}

	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') 
	{
		return -1; 
	}
	if(sscanf(&buf[2], "%d;%d", rows, columns) != 2) return -1;  
	editorReadKey();

	return -1;
}

// Append Buffer
struct abuf 
{
	char* b; 
	int len; 
};
#define ABUF_INIT {NULL, 0}


void abAppend(struct abuf* buf, const char* s, int len)
{
	char* new_s = reinterpret_cast<char*>(realloc(buf->b, buf->len + len)); 
	if (new_s == NULL) 
	{
		return;
	}
	memcpy(&new_s[buf->len], s, len);
	buf->b = reinterpret_cast<char*>(new_s);
	buf->len += len;

}


void abFree(struct abuf* buf)
{
	free(buf->b);
}



// OUTPUT
void editorDrawRows(struct abuf* buf)
{
	for(int y = 0; y < E.screenrows; y++)
	{
		abAppend(buf, "~", 1); 
		abAppend(buf, "\x1b[K", 3);
		if (y == E.screenrows / 3) 
		{
			char welcomeMessage[80];
			int welcomeLen = snprintf(welcomeMessage, sizeof(welcomeMessage), "Hex Editor -- version %s", VERSION);
			if (welcomeLen > E.screencolumns) 
			{
				welcomeLen = E.screencolumns;
			}
			int padding = (E.screencolumns - welcomeLen) / 2;
			if (padding) 
			{
				abAppend(buf, "~", 1); 
				padding -= 1;
			}
			while (padding--) 
			{
				abAppend(buf, " ", 1);
			}
			abAppend(buf, welcomeMessage, welcomeLen);
		}
		if (y < E.screenrows - 1) 
		{
			abAppend(buf, "\r\n", 2); 
		}
	}
}



void editorClearScreen()
{
	struct abuf buf = ABUF_INIT;
	abAppend(&buf, "\x1b[?25l", 6);
	abAppend(&buf, "\x1b[H",  3); 

	editorDrawRows(&buf); 
	char snBuffer[32];
	snprintf(snBuffer, sizeof(snBuffer), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);  
	abAppend(&buf, snBuffer, strlen(snBuffer));

	abAppend(&buf, "\x1b[?25h", 6);
	write(STDOUT_FILENO, buf.b, buf.len);
	abFree(&buf);
}



//INPUTS
void editorMoveCursor(int key)
{
	switch (key) 
	{
		case ARROW_UP:
			if (E.cy != 0) 
			{
				E.cy -= 1;
			}
			break;

		case ARROW_LEFT:
			if (E.screencolumns != 0) 
			{

				E.cx -= 1; 
			
			}
			break;

		case ARROW_DOWN: 
			if (E.cy != E.screenrows - 1) 
			{
				E.cy += 1; 
			}
			break;

		case ARROW_RIGHT:
			if (E.cy != E.screencolumns) 
			{
				E.cx += 1; 
			}
			break;
	}
}
void editorProcessKeyPress()
{
	int c = editorReadKey();

	switch (c) 
	{
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3); 
			exit(0); 
			break;

		case PAGE_UP:
		case PAGE_DOWN:
		{
			int times = E.screenrows;
			while (times--) 
			{
				editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;
		}
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}


int getWindowSize(int* row, int* column)
{
	struct winsize win;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == -1 || win.ws_col == 0) 
	{
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; 
		return getCursorPosition(row, column); 
	} 
	else 
	{
		*row = win.ws_row;
		*column = win.ws_col;
		return 0;
	}

}


//INIT
void initEditor()
{
	E.cx = 0;
	E.cy = 0;
	if(getWindowSize(&E.screenrows, &E.screencolumns) == -1 ) die("get window size"); 
}

int main()
{
	char c{};
	enableRawMode();
	initEditor();
	while (true)
	{
		editorClearScreen();
		editorProcessKeyPress();
		// if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
		// {
		// 	die("read"); 
		// }
		//
		// if (iscntrl(c)) 
		// {
		// 	printf("%d\r\n", c); 
		// } else {
		// 	printf("%d: (%c)\r\n", c, c); 
		// }
		// if (c == CTRL_KEY('q')) 
		// {
		// 	break;
		// }
	}
}

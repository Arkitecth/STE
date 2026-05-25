#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <termios.h>
#include <sys/ioctl.h>
#include "unistd.h"

//DEFINES
#define CTRL_KEY(k) ((k) & 0x1f)

//DATA
struct editorConfig
{
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

char editorReadKey()
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



// OUTPUT
void editorDrawRows()
{
	for(int y = 0; y < E.screenrows; y++)
	{
		write(STDOUT_FILENO,  "~", 1); 
		if (y < E.screenrows - 1) 
		{
			write(STDOUT_FILENO, "\r\n", 2); 
		}
	}
}



void editorClearScreen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4); 
	write(STDOUT_FILENO, "\x1b[H", 3); 

	editorDrawRows(); 
	write(STDOUT_FILENO, "\x1b[H", 3); 
}



//INPUTS
void editorProcessKeyPress()
{
	char c = editorReadKey();

	switch (c) 
	{
		case CTRL_KEY('q'):
			editorDrawRows();
			exit(0); 
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
	getWindowSize(&E.screenrows, &E.screencolumns); 
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

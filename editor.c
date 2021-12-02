//MIGHT HAVE TO DELETE SOME STUFF IN THE MAIN METHOD, THE "CORRECT" MAIN METHOD IS 
// IN THE SECOND SECTION OF THE TEXT EDITOR CODE, NEAR THE TOP "Clear the screen"
/*** INCLUDES ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include<termios.h>
#include <unistd.h>

/*** DEFINES ***/
//Mapping ctrl-q as quit
#define CTRL_KEY(k) ((k) & 0x1f)

/*** DATA ***/
struct editorConfig{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** TERMINAL ***/
//allows for error handling
void die(const char *s) {
    perror(s);
    exit(1);
}
//turning off rawmode/ echoing when the program exits
void disableRawMode(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}
//Turn off echoing, meaning you enable raw mode
void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    //allows terminate/suspend signals to be read(ctrl-s, ctrl-q) as bytes
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    //fixes newline charactesr
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    //ICANON flag turns off canonical mode, ISIG turns off terminate/suspend signals
    //IEXTEN disables ctrl-v and they now can be read as bytes
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcgetattr");
}

char editorReadKey(){
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) -1){
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols){
    struct winsize ws;
    //ioctl() places the number of columns and rows into the winsize struct
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        //moving the cursor right and down large values (999) to ensure it reaches
        //the very bottom right edge of the screen
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows,cols);
    }
    else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** APPEND BUFFER ***/
//creating a dynamic string type
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
    //asks memory to give a block large enough to store the string
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

//frees the memory requested by abAppend
void abFree(struct abuf *ab){
    free(ab->b);
}

/*** OUTPUT ***/

//print tilde's at the top like VIM
void editorDrawRows(struct abuf *ab) {
    int y; 
    for (y = 0; y < E.screenrows; y++) {
        abAppend(ab, "~", 1);

        if (y < E.screenrows -1){
            abAppend(ab, "\r\n", 2);
        }
    }
}
//clear the screen
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;
    //writing 4 bytes out to the terminal, escape character and some other command
    //COMMAND: ESC-J clears the screen to the cursor
    abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H", 3)

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
/*** INPUT ***/
void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
        //COMMAND: ctrl-q to quit
        case CTRL_KEY('q'):
            //clearing the screen upon exit
            write(STDOUT_FILENO, "\x1b[2j", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*** INIT ***/
//if there is an error with getting the window size
void initEditor(){
    if(getWindowsSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}
int main() {
    enableRawMode();

    while(1){
        editorRefreshScreen();
        editorProcessKeypress();
        char c = '\0';
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        //iscntrl() checks if a character is a control character
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c);
        }
        if(c == CTRL_KEY('q'))
            break;
    }
    //reads 1 byte from standard input into char c, as long it is not 'q'
    return 0;
}
//START HERE: chapter4: step 67
/*** INCLUDES ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** DEFINES ***/
//Mapping ctrl-q as quit
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8

enum editorKey {
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

/*** DATA ***/
typedef struct erow{
    int size;
    int rsize;
    char *chars;
    char *render;
} erow;

struct editorConfig{
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    erow *row;
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

int editorReadKey(){
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if(c =='\x1b'){
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[0] >= '0' && seq[1] <= '9'){
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~'){
                    switch (seq[1]){
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else{
                switch (seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if(seq[0] == '0'){
            switch (seq[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }    
    return '\x1b';
    }
    else{
        return c;
    }
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

/*** ROW OPERATIONS ***/

int editorRowCxToRx(erow *row, int cx){
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++){
        if (row->chars[j] == '\t'){
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        }
        rx++;
    }
    return rx;
}

void editorUpdateRow(erow *row){
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++){
        if (row->chars[j] == '\t'){
            tabs++;
        }
    }
    free(row->render);
    row->render = malloc(row->size + tabs*(KILO_TAB_STOP -1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++){
        if (row->chars[j] = '\t'){
            row->render[idx++] = ' ';
            while (idx % KILO_TAB_STOP != 0){
                row->render[idx++] = ' ';
            }
        }
        else{
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len){
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    
    E.numrows++;
    
}

/*** FILE I/O ***/

void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1){
        while (linelen> 0 && (line[linelen-1] == '\n' || line[linelen -1] == '\r')){
            linelen--;  
        }
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
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

void editorScroll(){
    E.rx = 0;
    if (E.cy < E.numrows){
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }
    if (E.cy < E.rowoff){
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows){
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.cx < E.coloff){
        E.coloff = E.rx;
    }
    if (E.cx >= E.coloff + E.screencols){
        E.coloff = E.rx - E.screencols + 1;
    }
}

//print tilde's at the top like VIM
void editorDrawRows(struct abuf *ab){
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } 
            else {
                abAppend(ab, "~", 1);
            }
        } 
        else {
            int len = E.row[filerow].rsize - E.coloff;
            //in case the user scrolls horizontally past the end of the line
            if (len < 0){
                len = 0;
            }
            if (len > E.screencols) {
                len = E.screencols;
            }
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);    
  }
}

void editorDrawStatusBar(struct abuf *ab){
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines", E.filename ? E.filename : "[No Name]", E.numrows);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
    if (len > E.screencols){
        len = E.screencols;
    }
    abAppend(ab, status, len);
    while (len < E.screencols){
        if (E.screencols - len == rlen){
            abAppend(ab, rstatus, rlen);
            break;
        }
        else{
            abAppend(ab, " ", 1);
            len++;
        }  
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab){
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols){
        msglen = E.screencols;
    }
    if (msglen && time(NULL) - E.statusmsg_time < 5){
        abAppend(ab, E.statusmsg, msglen);
    }
}

//clear the screen
void editorRefreshScreen() {
    editorScroll();
    struct abuf ab = ABUF_INIT;
    //writing 4 bytes out to the terminal, escape character and some other command
    //COMMAND: ESC-J clears the screen to the cursor
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}
/*** INPUT ***/

void editorMoveCursor(int key){
    erow *row  = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key) {
        case ARROW_LEFT:
            if (E.cx != 0){
                E.cx--;
            }
            else if (E.cy > 0){
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size){
                E.cx++;
            }
            else if (row && E.cx == row->size){
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0){
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows){
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    //snap cursor to the end of the line
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}
void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        //COMMAND: ctrl-q to quit
        case CTRL_KEY('q'):
            //clearing the screen upon exit
            write(STDOUT_FILENO, "\x1b[2j", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        
        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            if (E.cy < E.numrows){
                E.cx = E.row[E.cy].size;
            }
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (c == PAGE_UP){
                    E.cy - E.rowoff;
                }
                else if (c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if (E.cy > E.numrows){
                        E.cy = E.numrows;
                    }
                }
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

/*** INIT ***/
//if there is an error with getting the window size
void initEditor(){
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL
    E.filename - NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screencols -= 2;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2){
        editorOpen();
    }

    editorSetStatusMessage("HELP: Ctrl-q = quit");
    
    while(1){
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;

    /*while(1){
       
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
    */
    //reads 1 byte from standard input into char c, as long it is not 'q'
    
}
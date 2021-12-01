/*** INCLUDES ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include<termios.h>
#include <unistd.h>

/*** DATA ***/
struct termios orig_termios;

/*** TERMINAL ***/
//allows for error handling
void die(const char *s) {
    perror(s);
    exit(1);
}
//turning off rawmode/ echoing when the program exits
void disableRawMode(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}
//Turn off echoing, meaning you enable raw mode
void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr"));
    atexit(disableRawMode);

    struct termios raw = orig_termios;
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

/*** INIT ***/
int main() {
    enableRawMode();
    while(1){
        char c '\0'
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        //iscntrl() checks if a character is a control character
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c);
        }
        if(c == 'q')
            break;
    }
    //reads 1 byte from standard input into char c, as long it is not 'q'
    return 0;
}
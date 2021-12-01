#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include<termios.h>
#include <unistd.h>

struct termios orig_termios;
 
//turning off rawmode/ echoing when the program exits
void disableRawMode(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
//Turn off echoing, meaning you enable raw mode
void enableRawMode(){
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    //ICANON flag turns off canonical mode
    raw.c_lflag &= ~(ECHO | ICANON);

    tcsetattri(STDIN_FILENO, TCSAFLUSH, $raw);
}
int main() {
    enableRawMode();
    char c;
    //reads 1 byte from standard input into char c, as long it is not 'q'
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q'){
        //iscntrl() checks if a character is a control character
        if (iscntrl(c)) {
            printf("%d\n", c);
        }
        else {
            printf("%d ('%c')\n", c, c);
        }
    }
    return 0;
}
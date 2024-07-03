/*** includes ***/
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <errno.h>
#include<unistd.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<ctype.h>
#include<termios.h>
#include<stdio.h>
#include<stdlib.h>
/*defines*/
#define CTRL_KEY(k) ((k) & 0x1f)
/*** data ***/
struct editorConfig{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};
struct editorConfig E;
/***terminal***/
void die(const char *s){
    write(STDOUT_FILENO, "\x1b[2J" , 4);
    write(STDOUT_FILENO , "\x1b[H" , 3);
    perror(s);
    exit(1);
}
void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}
void enableRawMode(){
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr"); 
    atexit(disableRawMode);
    
    struct termios raw = E.orig_termios;
    raw.c_lflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~(ICRNL | IXON);
    raw.c_lflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // this is a genius piece of code right here
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}
char editorReadKey(){
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}
int getCursorPosition(int *rows, int *cols){
    if(write(STDOUT_FILENO, "\x1b[6n", 4) !=4) return -1;

    printf("\r\n");
    char c;
    while(read(STDIN_FILENO, &c, 1) == 1){
        if (iscntrl(c)){
            printf("%d \r\n",c);
        }
        else{
            printf("%d ('%c')\r\n",c,c);
        }
    }
    editorReadKey();
    return -1;
    }
int getWindowsSize(int *rows, int *cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
        return getCursorPosition(rows,cols);
        } 
    } else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
/* output */
void editorDrawRows(){
    int y;
    for(y = 0; y<E.screenrows; y++){
        write(STDOUT_FILENO, "=\r\n",3);
    }
}
void editorRefreshScreen(){ 
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
   // write(STDOUT_FILENO, " Ujjwal Kala Samjha Kya bidhu", 30);/* it comes out to be 30 as there are 29 characters and last character is ending character so ek byte uski */
}
/*input */
void editorProcessKeypress(){
    char c = editorReadKey();
    switch(c){
        case CTRL_KEY('c'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}
/*** init ***/
void initEditor(){
    if(getWindowsSize(&E.screenrows, &E.screencols ) == -1) die("getWindowSize");
}
int main(){
    enableRawMode();
    initEditor();
    char c;
    while(1){
        editorRefreshScreen(); 
        editorProcessKeypress();
    }
    return 0;
}

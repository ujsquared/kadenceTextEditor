/*** includes ***/
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <errno.h>
#include<unistd.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<string.h>
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
    char buf[32];
    unsigned int i = 0 ;

    if(write(STDOUT_FILENO, "\x1b[6n", 4) !=4) return -1;

    while(i< sizeof(buf) -1){
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) !=2) return -1;
    return 0;
}
int getWindowsSize(int *rows, int *cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
            return -1;
        } 
        return getCursorPosition(rows, cols); 
    } else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
/*append buffer */
// this section creates functions to implement the functionality of dynamic strings to our program
// this is important because instead of using multiple write function which may cause flicker issue, we will use single write 
// on a string that we'll keep adding and mutating. we'll call this string a buffer. 

struct abuf{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b , ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s,len);
    ab->b = new;
    ab->len += len;
}
void abFree(struct abuf *ab){
    free(ab->b);
}

/* output */
void editorDrawRows(struct abuf *ab){
    int y;
    for(y = 0; y<E.screenrows; y++){
        abAppend(ab, "=", 1);
        if(y < E.screenrows - 1){
            abAppend(ab, "\r\n", 2);
        }
    }
}
void editorRefreshScreen(){ 
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab);
    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, " Ujjwal Kala Samjha Kya bidhu", 30);/* it comes out to be 30 as there are 29 characters and last character is ending character so ek byte uski */

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
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

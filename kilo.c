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
#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey{
    ARROW_LEFT = 1000 ,
    ARROW_UP ,
    ARROW_RIGHT, 
    ARROW_DOWN 
};
/*** data ***/
struct editorConfig{
    int cx,cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};
// one important thing to note down, we have not handled the case of cursor going out of bounds, 
// in that case when the cursor crosses the boundary the terminal sets is back to 1,1/0,0 screenrows,col position
// this is done by the terminal emulator im guessing, might be wrong later (next part is handling that case)
//
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
int editorReadKey(){
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    if( c == '\x1b'){
        char seq[3];
        
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if(seq[0] == '['){
            switch(seq[1]){
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT ;
                case 'D': return ARROW_LEFT; 
            // here case ABCD represents up,down right left respectively due to escape sequence in VT100 terminals being coded
            // that way.
            }
        }
        return '\x1b';
    } else{
    return c;
    }
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
// existence of a function named drawRows symmetrically implies existence of functionnamed DrawColumns, add it
// Make it a box boys 
void editorDrawRows(struct abuf *ab){
    int y;
    for(y = 0; y<E.screenrows; y++){
        if (y == E.screenrows / 3){
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Kilo editor -- versions %s",KILO_VERSION);
            if(welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if(padding) {
                abAppend(ab,"=", 1);
                padding--;
            }
            while(padding--) abAppend(ab," ", 1);
            abAppend(ab, welcome, welcomelen);
        } else{
        abAppend(ab, "=", 1);
        //if(y == E.screenrows - 1){
        //    abAppend(ab,"This definitely be the last line ig?",37);
        }
        abAppend(ab, "\x1b[K", 3);
        if(y < E.screenrows - 1){
            abAppend(ab, "\n", 2); // what the fuck does the slash r do????? it literally gives the same results overalllllllllllllll
        }
        
    }
}
void editorRefreshScreen(){ 
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // turning off the visibility of the cursor
    abAppend(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab);
    abAppend(&ab, "\x1b[H", 3);
    char buf[32];
    snprintf(buf, sizeof(buf),"\x1b[%d;%dH" ,E.cy+1,E.cx+1 );//
    abAppend(&ab, buf, strlen(buf));
    //abAppend(&ab, " Ujjwal Kala Samjha Kya bidhu", 30);/* it comes out to be 30 as there are 29 characters and last character is ending character so ek byte uski */
    abAppend(&ab, "\x1b[?25h", 6); // turning on the visibility of the cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
/*input */
void editorMoveCursor(int key){
    switch(key){
        case ARROW_LEFT:
            if(E.cx!=0) E.cx--;
            break;
        case ARROW_DOWN:
            if(E.cy != E.screenrows-1) E.cy++;
            break;
        case ARROW_RIGHT:
            if(E.cx != E.screencols-1) E.cx++;
            break;
        case ARROW_UP:
            if(E.cy != 0) E.cy--;
            break;
    }
}
void editorProcessKeypress(){
    int c = editorReadKey();
    switch(c){
        case CTRL_KEY('c'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}
/*** init ***/
void initEditor(){
    E.cx = 10;
    E.cy = 10;
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

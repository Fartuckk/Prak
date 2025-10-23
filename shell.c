#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** cmds = NULL;
int cmdslen = 0;

enum {
    SPACE,
    REGULAR,
    QUOTE
}  state;

/*temporary function for debug*/
void Debug() {
    printf("state = %d\ncmdslen = %d\n", state, cmdslen);
    // for (int i = 0; i < cmdslen; ++i) {
    //     printf("Cmd %d:  %s\n", i, cmds[i]);
    // }
}

/*returns 1 if char is "space-like", otherwise returns 0*/
int Is_space(char ch) {
    return ( ((ch >= 0) && (ch <= 32)) || (ch == 127) );
}

/*returns 1 if char is considered "special symbol", otherwise returns 0*/
int Is_special(char ch) {
    return ( (ch == '>') || (ch == '<') || (ch == '!') || (ch == '&') || (ch == '$') || (ch == '^') || (ch == ';') || (ch == ':') || (ch == ',') );
}

/*frees all memory used for CMDS, resets CMDSLEN and STATE*/
void ClearCmds() {
    for (int i = 0; i < cmdslen; ++i) {
        free(cmds[i]);
    }
    free(cmds);
    cmds = NULL;
    cmdslen = 0;
    state = SPACE;
}

void PutNewWord(char ch) {
    ++cmdslen;
    cmds = realloc(cmds, cmdslen*sizeof(char *));
    cmds[cmdslen-1] = malloc(2*sizeof(char));
    cmds[cmdslen-1][0] = ch;
    cmds[cmdslen-1][1] = '\0';
}

void PutCurWord(char ch) {
    int len = strlen(cmds[cmdslen-1]);
    cmds[cmdslen-1] = realloc(cmds[cmdslen-1], (len + 2)*sizeof(char));
    cmds[cmdslen-1][len] = ch;
    cmds[cmdslen-1][len + 1] = '\0';
}

/*parser by char*/
void Parser(char ch) {
    switch (state) {
        case SPACE:
            if ( Is_space(ch) ) {
                /*doing nothing*/
            }
            else if (Is_special(ch))
            {
                PutNewWord(ch);
                state = SPACE;
            }
            else if ( (1) ) {
                PutNewWord(ch);
                state = REGULAR;
            }
        break;
        case REGULAR:
            if ( Is_space(ch) ) {
                state = SPACE;
            }
            else if ( Is_special(ch) ) {
                PutNewWord(ch);
                state = SPACE;
            }
            else if ( (1) ) {
                PutCurWord(ch);
            }
        break;
        case QUOTE:
            
        break;
    }
}

/*execute parsered cmds*/
void Execute() {
    for (int i = 0; i < cmdslen; ++i) {
        printf("Word %d:  %s\n", i, cmds[i]);
    }
    printf("Executed.\n");
    ClearCmds();
}

int main() {
    char ch;
    state = SPACE;

    while (1) {
        ch = getchar();
        if (ch == EOF) {
            printf("\nExit.\n");
            return 0;
        }
        if (ch == '\n') {
            Execute();
        } else {
            Parser(ch);
        }
    }
}
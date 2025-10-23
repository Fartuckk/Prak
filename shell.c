#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** cmds = NULL;
int cmdslen = 0;

enum {
    SPACE,
    REGULAR
}  state;

/*returns 1 if char is "space-like", otherwise returns 0*/
int Is_space(char ch) {
    return ( ((ch >= 0) && (ch <= 32)) || (ch == 127) );
}

/*temporary function for debug*/
void Debug() {
    printf("state = %d\ncmdslen = %d\n", state, cmdslen);
    // for (int i = 0; i < cmdslen; ++i) {
    //     printf("Cmd %d:  %s\n", i, cmds[i]);
    // }
}

/*parser by char*/
void Parser(char ch) {
    switch (state) {
        case SPACE:
            if ( Is_space(ch) ) {
                /*doing nothing*/
            }
            else if ( (1) ) {
                ++cmdslen;
                cmds = realloc(cmds, cmdslen*sizeof(char *));
                cmds[cmdslen-1] = malloc(2*sizeof(char));
                cmds[cmdslen-1][0] = ch;
                cmds[cmdslen-1][1] = '\0';
                state = REGULAR;
            }
        break;
        case REGULAR:
            if ( Is_space(ch) ) {
                state = SPACE;
            }
            else if ( (1) ) {
                cmds[cmdslen-1] = realloc(cmds[cmdslen-1], (strlen(cmds[cmdslen-1]) + 2)*sizeof(char));
                cmds[cmdslen-1][strlen(cmds[cmdslen-1])] = ch;
                cmds[cmdslen-1][strlen(cmds[cmdslen-1]) + 1] = '\0';
            }
        break;
    }
}

/*execute parsered cmds*/
void Execute() {
    for (int i = 0; i < cmdslen; ++i) {
        printf("Word %d:  %s\n", i, cmds[i]);
    }
    printf("Executed.\n");
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
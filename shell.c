#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** cmds = NULL;
int cmdslen = 0;

enum  {
    SPACE,
    REGULAR
}  state;

inline int Is_space(char ch) {
    if ( ((ch >= 0) && (ch <= 32)) || (ch == 127) ) {
        return 1;
    } else {
        return 0;
    }
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
                cmds[cmdslen-1] = malloc((strlen(cmds[cmdslen-1]) + 2)*sizeof(char));
                cmds[cmdslen-1][strlen(cmds[cmdslen-1])] = ch;
                cmds[cmdslen-1][strlen(cmds[cmdslen-1]) + 1] = '\0';
            }
        break;
    }
}

/*execute parsered cmds*/
void Execute() {
    printf("Executed.\n");
}

int main() {
    char ch;

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
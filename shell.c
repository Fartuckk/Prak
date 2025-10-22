#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** cmds = NULL;
int cmdslen = 0;

enum  {
    SPACE,
    REGULAR
}  state;

/*parser by char*/
void Parser(char ch) {
    switch (state) {
        case SPACE:
            if ( ((ch >= 0) && (ch <= 32)) || (ch == 127)) {
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
            if ( ((ch >= 0) && (ch <= 32)) || (ch == 127)) {
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
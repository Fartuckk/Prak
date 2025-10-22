#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** cmds = NULL;

/*parser by char*/
void Parser(char ch) {
    printf("parsed symbol %c\n", ch);
}

/*execute parsered cmds*/
void Execute() {
    printf("Executed.\n");
}

int main() {
    char ch;

    while ( 1 ) {
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
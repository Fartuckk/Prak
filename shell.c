#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** cmds = NULL;

void Parser() {
}

int main() {
    char ch;

    ch = getchar();
    while (ch != EOF) {
        while ( (ch != '\n') && (ch != EOF) ) {
            

            ch = getchar();
        }
    }

    return 0;
}
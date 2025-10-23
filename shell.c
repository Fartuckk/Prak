#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

char** cmds = NULL;  /*переделать в новую структуру, носить cmdslen с собой*/
int cmdslen = 0;
int* prs = NULL;    /*список процессов, запущенных на фоне, хранятся в виде pid'ов*/
int prscount = 0;

enum {
    SPACE,
    REGULAR,
    QUOTE
} state = SPACE;

/*temporary function for debug*/
void Debug(void) {
    printf("state = %d\ncmdslen = %d\n", state, cmdslen);
    // for (int i = 0; i < cmdslen; ++i) {
    //     printf("Cmd %d:  %s\n", i, cmds[i]);
    // }
}

/*returns 1 if function is ready to execute, otherwise returns 0*/
int Is_ready2execute(void) {
    return ( (state != QUOTE) );
}

/*returns 1 if char is considered "space-like", otherwise returns 0*/
int Is_space(char ch) {
    return ( ((ch >= 0) && (ch <= 32)) || (ch == 127) );
}

/*returns 1 if char is considered "special symbol", otherwise returns 0*/
int Is_special(char ch) {
    return ( (ch == '>') || (ch == '<') || (ch == '!') || (ch == '&') || (ch == '$') || (ch == '^') || (ch == ';') || (ch == ':') || (ch == ',') );
}

/*returns 1 if char is "quote", otherwise returns 0*/
int Is_quote(char ch) {
    return (ch == '"');
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

void Put2NewWord(char ch) {
    ++cmdslen;
    cmds = realloc(cmds, cmdslen*sizeof(char *));
    if (ch == 0) {                               // начало нового пустого слова
        cmds[cmdslen-1] = malloc(sizeof(char));
        cmds[cmdslen-1][0] = '\0';
    }
    else {                                     // начало нового слова из 1 символа
        cmds[cmdslen-1] = malloc(2*sizeof(char));
        cmds[cmdslen-1][0] = ch;
        cmds[cmdslen-1][1] = '\0';
    }
}

void Put2CurWord(char ch) {
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
            else if ( Is_special(ch))
            {
                Put2NewWord(ch);
                state = SPACE;
            }
            else if ( Is_quote(ch) ) {
                Put2NewWord(0);
                state = QUOTE;
            }
            else { // is regular.
                Put2NewWord(ch);
                state = REGULAR;
            }
        break;
        case REGULAR:
            if ( Is_space(ch) ) {
                state = SPACE;
            }
            else if ( Is_special(ch) ) {
                Put2NewWord(ch);
                state = SPACE;
            }
            else if ( Is_quote(ch) ) {
                state = QUOTE;
            }
            else { // is regular.
                Put2CurWord(ch);
            }
        break;
        case QUOTE:
            if ( Is_quote(ch) ) {
                state = REGULAR;
            }
            else { // is special, space, '\n', regular
                Put2CurWord(ch);
            }
        break;
    }
}

/*execute parsered cmds*/
void Execute() {
    cmds = realloc(cmds, (cmdslen+1)*sizeof(char *));
    cmds[cmdslen] = NULL;
    char *s;

    if (!strcmp(cmds[0], "cd")) {                // "cd" command handler
        if (chdir(cmds[1]) == -1) {
            if (cmds[1] == NULL) {
                ClearCmds();
                return;
            }
            perror("cd failed");
        }
        ClearCmds();
        return;
    }

    pid_t pid = fork();
    int status;
    if (pid == 0) {               // SON
        printf("Executing...\n");
        execvp(cmds[0], cmds);
        // printf("%s\n", strerror(errno));      // analog to perror()
        s = cmds[0];
        perror(s);
        exit(1);
    }
    else if (pid != -1) {       // FATHER
        /*добавить проверку на & в конце*/
        if(waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }
        printf("Executed.\n");
    }
    else {
        perror("FORK FAILED.");
    }
    ClearCmds();
}

int main() {
    char ch;
    char cwd[1024] = "$";
    getcwd(cwd, sizeof(cwd));
    printf("%s$ ", cwd);

    while (1) {
        ch = getchar(); /*заменить на неблокирующий вариант getchar'а*/
        /*здеся будет проверка всех фоновых процессов: for до prscount и waitpid каждому prs[i]*/
        if (ch == EOF) {
            printf("\nExit.\n");
            return 0;
        }
        if ( (ch == '\n') && (Is_ready2execute()) ) {
            Execute();
            getcwd(cwd, sizeof(cwd));
            printf("%s$ ", cwd);
        }
        else {
            Parser(ch);
        }
    }
}
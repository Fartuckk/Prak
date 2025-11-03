#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

// char** cmds = NULL;  /*переделать в новую структуру, носить cmdslen с собой*/
// int cmdslen = 0;

typedef struct {
    char ** cmds;
    int cmdslen;
} cmnd;

cmnd* command = NULL;
int commandcount = 0;

int* prs = NULL;    // список фоновых процессов, хранятся в виде pid'ов
int prscount = 0;

enum {
    SPACE,
    REGULAR,
    QUOTE
} state = SPACE;

// temporary function for debug
void Debug(void) {
    printf("state = %d\ncmdslen = %d\n", state, command[commandcount-1].cmdslen);
    // for (int i = 0; i < command[commandcount-1].cmdslen; ++i) {
    //     printf("Cmd %d:  %s\n", i, command[commandcount-1].cmds[i]);
    // }
}

// returns 1 if function is ready to execute, otherwise returns 0
int Is_ready2execute(void) {
    return ( (state != QUOTE) );
}

// returns 1 if char is considered "space-like", otherwise returns 0
int Is_space(char ch) {
    return ( ((ch >= 0) && (ch <= 32)) || (ch == 127) );
}

// returns 1 if char is considered "special symbol", otherwise returns 0
int Is_special(char ch) {
    return ( (ch == '>') || (ch == '<') || (ch == '!') || (ch == '&') || (ch == '$') || (ch == '^') || (ch == ';') || (ch == ':') || (ch == ',') );
}

// returns 1 if char is "quote", otherwise returns 0
int Is_quote(char ch) {
    return (ch == '"');
}

// returns 1 if char is "semicolon", otherwise returns 0
int Is_semicolon(char ch) {
    return (ch == ';');
}

// frees all memory used for command[k], resets CMDSLEN and STATE
void ClearCmds(cmnd* command) {
    for (int i=0; i<command->cmdslen; ++i) {
        free(command->cmds[i]);
    }
    free(command->cmds);
    command->cmds = NULL;
    command->cmdslen = 0;
}

// clears all command
void ClearAll(void) {
    for(int i=0; i<commandcount; i++)
        ClearCmds(command+i);
    free(command);
    state = SPACE;
}

void Put2NewWord(char ch) {
    cmnd currcomm = command[commandcount-1];
    char* lastcmd = currcomm.cmds[currcomm.cmdslen-1];
    int len = strlen( lastcmd );

    ++currcomm.cmdslen;
    currcomm.cmds = realloc(currcomm.cmds, currcomm.cmdslen*sizeof(char *));
    if (ch == 0) {                               // начало нового пустого слова
        lastcmd = malloc(sizeof(char));
        lastcmd[0] = '\0';
    }
    else {                                     // начало нового слова из 1 символа
        lastcmd = malloc(2*sizeof(char));
        lastcmd[0] = ch;
        lastcmd[1] = '\0';
    }
}

void Put2CurWord(char ch) {
    cmnd currcomm = command[commandcount-1];
    char* lastcmd = currcomm.cmds[currcomm.cmdslen-1];
    int len = strlen( lastcmd );

    lastcmd = realloc(lastcmd, (len + 2)*sizeof(char));
    lastcmd[len] = ch;
    lastcmd[len + 1] = '\0';
}

// parser by char
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
            else if ( Is_semicolon(ch) ) {
                ++commandcount;
                realloc(command, commandcount*sizeof(cmnd));
                state = SPACE;
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
            else if ( Is_semicolon(ch) ) {
                ++commandcount;
                realloc(command, commandcount*sizeof(cmnd));
                state = SPACE;
            }
            else { // is regular.
                Put2CurWord(ch);
            }
        break;
        case QUOTE:
            if ( Is_quote(ch) ) {
                state = REGULAR;
            }
            else { // is special, space, semicolon, '\n', regular
                Put2CurWord(ch);
            }
        break;
    }
}

// execute parsered cmds
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
        execvp(cmds[0], cmds);
        // printf("%s\n", strerror(errno));      // analog to perror()
        s = cmds[0];
        perror(s);
        exit(1);
    }
    else if (pid != -1) {       // FATHER
        if (1) {                                 // добавить проверку на & в конце
            if(waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
            }
        }
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
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
    return ( (ch == '>') || (ch == '<') || (ch == '!') || (ch == '&') || (ch == '$') || (ch == '^') || (ch == ':') || (ch == ',') );
}

// returns 1 if char is "quote", otherwise returns 0
int Is_quote(char ch) {
    return (ch == '"');
}

// returns 1 if char is "semicolon", otherwise returns 0
int Is_semicolon(char ch) {
    return (ch == ';');
}

// frees all memory used for command_i
void ClearCmds(cmnd* command_i) {
    for (int i=0; i<command_i->cmdslen; ++i) {
        free(command_i->cmds[i]);
    }
    free(command_i->cmds);
    command_i->cmds = NULL;
    command_i->cmdslen = 0;
}

// clears all command if mode = 0; also mallocs command if mode = 1
void ClearAll(int mode) {
    for(int i=0; i<commandcount; i++) {
        ClearCmds(command+i);
    }
    free(command);
    if (mode) {
        command = malloc(sizeof(cmnd));
        command->cmds = NULL;
        command->cmdslen = 0;
        commandcount = 1;
    }
}

void Put2NewWord(char ch) {
    cmnd* currcomm = command+commandcount-1;
    ++currcomm->cmdslen;
    currcomm->cmds = realloc(currcomm->cmds, currcomm->cmdslen*sizeof(char *));

    if (ch == 0) {                               // начало нового пустого слова (только для quote)
        currcomm->cmds[currcomm->cmdslen-1] = malloc(sizeof(char));
        currcomm->cmds[currcomm->cmdslen-1][0] = '\0';
    }
    else {                                     // начало нового слова из 1 символа
        currcomm->cmds[currcomm->cmdslen-1] = malloc(2*sizeof(char));
        currcomm->cmds[currcomm->cmdslen-1][0] = ch;
        currcomm->cmds[currcomm->cmdslen-1][1] = '\0';
    }
}

void Put2CurWord(char ch) {
    cmnd* currcomm = command+commandcount-1;
    int len = strlen( currcomm->cmds[currcomm->cmdslen-1] );

    currcomm->cmds[currcomm->cmdslen-1] = realloc(currcomm->cmds[currcomm->cmdslen-1], (len + 2)*sizeof(char));
    currcomm->cmds[currcomm->cmdslen-1][len] = ch;
    currcomm->cmds[currcomm->cmdslen-1][len + 1] = '\0';
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
                command = realloc(command, commandcount*sizeof(cmnd));
                (command+commandcount-1)->cmds = NULL;
                (command+commandcount-1)->cmdslen = 0;
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
                printf("%d", commandcount);
                command = realloc(command, commandcount*sizeof(cmnd));
                (command+commandcount-1)->cmds = NULL;
                (command+commandcount-1)->cmdslen = 0;
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
// void Execute() {
    
    
    
    
    
    
//     cmds = realloc(cmds, (cmdslen+1)*sizeof(char *));
//     cmds[cmdslen] = NULL;
//     char *s;

//     if (!strcmp(cmds[0], "cd")) {                // "cd" command handler
//         if (chdir(cmds[1]) == -1) {
//             if (cmds[1] == NULL) {
//                 ClearAll(1);
//                 return;
//             }
//             perror("cd failed");
//         }
//         ClearAll(1);
//         return;
//     }

//     pid_t pid = fork();
//     int status;
//     if (pid == 0) {               // SON
//         execvp(cmds[0], cmds);
//         // printf("%s\n", strerror(errno));      // analog to perror()
//         s = cmds[0];
//         perror(s);
//         exit(1);
//     }
//     else if (pid != -1) {       // FATHER
//         if (1) {                                 // добавить проверку на & в конце
//             if(waitpid(pid, &status, 0) == -1) {
//                 perror("waitpid");
//             }
//         }
//     }
//     else {
//         perror("FORK FAILED.");
//     }
//     ClearAll(1);
// }

// temporary function for debug
void Debug(void) {
    for (int i=0; i<commandcount; ++i) {
        printf("COMMAND %d:\n", i);
        for (int j=0; j<command[i].cmdslen; ++j) {
            printf("     cmd[%d]:   %s\n", j, command[i].cmds[j]);
        }
        printf("\n");
    }
    ClearAll(1);
    // printf("state = %d\ncmdslen = %d\n", state, command[commandcount-1].cmdslen);
    // for (int i = 0; i < command[commandcount-1].cmdslen; ++i) {
    //     printf("Cmd %d:  %s\n", i, command[commandcount-1].cmds[i]);
    // }
}



int main() {
    char ch;
    char cwd[1024] = "$";
    getcwd(cwd, sizeof(cwd)); printf("%s$ ", cwd); // вывод строки текущей папки

    ClearAll(1); // по сути, malloc(command) и обнуление его частей

    while (1) {
        ch = getchar(); /*заменить на неблокирующий вариант getchar'а*/
        /*здеся будет проверка всех фоновых процессов: for до prscount и waitpid каждому prs[i]*/
        if (ch == EOF) {
            printf("\nExit.\n");
            return 0;
        }
        if ( (ch == '\n') && (Is_ready2execute()) ) {
            // Execute();
            Debug();

            state = SPACE;
            getcwd(cwd, sizeof(cwd)); printf("%s$ ", cwd); // вывод строки текущей папки
        }
        else {
            Parser(ch);
        }
    }
}
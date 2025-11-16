#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>



int group_flag = 0;


typedef struct {
    char** cmds;
    int cmdslen;
} cmnd;

// cmnd* command = NULL;
// int commandcount = 0;

typedef struct {
    cmnd* command;
    int commandcount;
} group;

group* grp = NULL;
int grpcount = 0;



int* prs = NULL;    // list of background processes, contains their pids
int prscount = 0;

enum {
    SPACE,
    REGULAR,
    QUOTE,
    PIPE
} state = SPACE;




// creates new command and increases commandcount
void CreateNewCommand(void) {
    ++grp[grpcount-1].commandcount;
    grp[grpcount-1].command = realloc(grp[grpcount-1].command, grp[grpcount-1].commandcount * sizeof(cmnd));
    (grp[grpcount-1].command + grp[grpcount-1].commandcount - 1)->cmds = NULL;
    (grp[grpcount-1].command + grp[grpcount-1].commandcount - 1)->cmdslen = 0;
}

// creates new group and increases grpcount
void CreateNewGroup(void) {
    ++grpcount;
    grp = realloc(grp, grpcount*sizeof(group));

    grp[grpcount-1].commandcount = 0;
    grp[grpcount-1].command = NULL;
    CreateNewCommand();
}

// frees all memory used for command_i, to be used in ClearGroup() only
void ClearCmds(cmnd* command_i) {
    for (int i=0; i<command_i->cmdslen; ++i) {
        free(command_i->cmds[i]);
        command_i->cmds[i] = NULL;
    }
    free(command_i->cmds);
    command_i->cmds = NULL;
    command_i->cmdslen = 0;
}

// frees all memory used for group_i, to be used in ClearAll() only
void ClearGroup(group* grp_i) {
    for(int i=0; i<grp_i->commandcount; ++i) {
        ClearCmds(grp_i->command+i);
    }
    free(grp_i->command);
    grp_i->command = NULL;
}

// frees all groups if mode = 0; also mallocs 1st group and 1st command if mode = 1
void ClearAll(int mode) {
    for (int i=0; i<grpcount; ++i) {
        ClearGroup(grp+i);
    }
    free(grp);
    grp = NULL;
    grpcount = 0;
    if (mode) {
        CreateNewGroup();
    }
}




// is equivalent to getchar(), but is non-blocking.
// returns char if read successfully, 0 if nothing was read., -1 if EOF was read.
int read_nonblock() { /*при необходимости можно заменить на read_nonblock(char *c), для корректизации возвращаемых значений*/
    unsigned char c;
    int res;
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    res = read(STDIN_FILENO, &c, sizeof(char));
    if (res > 0) { // char read.
        return c;
    }
    else if (res == 0) { // EOF read.
        return EOF;
    }
    else { // nothing.
        return 0;
    }
}




// returns 1 if "s" is acceptable parameter for "cd" command, otherwise returns 0
int Check_cd_parameter(const char *s) {
    return !( strcmp(s, "-L") && strcmp(s, "-P") && strcmp(s, "-e") && strcmp(s, "-LP") && strcmp(s, "-PL") && strcmp(s, "-Le") && strcmp(s, "-eL") && strcmp(s, "-Pe") && strcmp(s, "-eP") && strcmp(s, "-LPe") && strcmp(s, "-LeP") && strcmp(s, "-ePL") && strcmp(s, "-eLP") && strcmp(s, "-PLe") && strcmp(s, "-PeL") );
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
    return ( (ch == '=') || (ch == '>') || (ch == '<') || (ch == '!') || (ch == '&') || (ch == '$') || (ch == '^') || (ch == ':') || (ch == ',') );
}

// returns 1 if char is "quote", otherwise returns 0
int Is_quote(char ch) {
    return (ch == '"');
}

// returns 1 if char is "semicolon", otherwise returns 0
int Is_semicolon(char ch) {
    return (ch == ';');
}

// returns 1 if char is "pipe", otherwise returns 0
int Is_pipe(char ch) {
    return (ch == '|');
}




// creates a new empty word if ch = 0, otherwise creates a new word adding ch to the beginning of it
void Put2NewWord(char ch) {
    cmnd* currcomm = grp[grpcount-1].command + grp[grpcount-1].commandcount-1;
    ++currcomm->cmdslen;
    currcomm->cmds = realloc(currcomm->cmds, currcomm->cmdslen*sizeof(char *));

    if (ch == 0) {                               // начало нового пустого слова (только для quote)
        currcomm->cmds[currcomm->cmdslen-1] = malloc(sizeof(char));
        currcomm->cmds[currcomm->cmdslen-1][0] = '\0';
    }
    else {                                       // начало нового слова из 1 символа
        currcomm->cmds[currcomm->cmdslen-1] = malloc(2*sizeof(char));
        currcomm->cmds[currcomm->cmdslen-1][0] = ch;
        currcomm->cmds[currcomm->cmdslen-1][1] = '\0';
    }
}

// adds ch to the end of last cmd
void Put2CurWord(char ch) {
    cmnd* currcomm = grp[grpcount-1].command + grp[grpcount-1].commandcount-1;
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
                // doing nothing
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
                CreateNewGroup();
                state = SPACE;
            }
            else if ( Is_pipe(ch) ) {
                state = PIPE;
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
                CreateNewGroup();
                state = SPACE;
            }
            else if ( Is_pipe(ch) ) {
                state = PIPE;
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
        case PIPE:
            if ( Is_space(ch) ) {
                // doing nothing
            }
            else if ( Is_special(ch) ) {
                CreateNewCommand();
                Put2NewWord(ch);
                state = SPACE;
            }
            else if ( Is_quote(ch) ) {
                CreateNewCommand();
                Put2NewWord(ch);
                state = QUOTE;
            }
            else if ( Is_semicolon(ch) ) {
                CreateNewGroup();
                state = SPACE;
            }
            else if ( Is_pipe(ch) ) {  // "||" logical operator, or error
                fflush(NULL);
                ClearAll(0);
                exit(1);
            }
            else { // is regular
                CreateNewCommand();
                Put2NewWord(ch);
                state = REGULAR;
            }
        break;
    }
}


// execute command[k], waiting till its completion if mode = 0, executing it at background while adding its pid to the prs* if mode = 1
// void ExecuteSingleCommand(int k, int mode) {
//     int len = command[k].cmdslen;
//     command[k].cmds = realloc(command[k].cmds, (len + 1)*sizeof(char *));
//     command[k].cmds[len] = NULL;

//     int dir = 1;
//     if (!strcmp(command[k].cmds[0], "cd")) {     // "cd" command handler
//         while ( (command[k].cmds[dir] != NULL) && (command[k].cmds[dir][0] == '-') ) {  // parameters for "cd" handler
//             if ( Check_cd_parameter(command[k].cmds[dir]) == 0 ) {
//                 printf("cd: invalid option\n");fflush(stdout);
//                 // ClearAll(1);
//                 return;
//             }
//             ++dir;
//         }

//         if (chdir(command[k].cmds[dir]) == -1) {
//             if (command[k].cmds[dir] == NULL) {
//                 // ClearAll(1);
//                 return;
//             }
//             perror("cd failed");
//         }
//         ClearAll(1);
//         return;
//     }

//     pid_t pid = fork();
//     int status;
//     char *s;
//     if (pid == 0) {                              // SON
//         execvp(command[k].cmds[0], command[k].cmds);
//         // we are here only if execvp exited with error
//         s = command[k].cmds[0];
//         perror(s);
//         exit(1);
//     }
//     else if (pid > 0) {                        // FATHER
//         // printf("AAAAA\n");fflush(stdout);
//         if (mode) {
//             prs = realloc(prs, (prscount+1)*sizeof(int *));
//             prs[prscount] = pid;
//             ++prscount;
//             printf("[%d] %d\n", prscount, pid);fflush(stdout);
//         }
//         else {
//             if (waitpid(pid, &status, 0) == -1) {
//                 perror("waitpid");
//             }
//         }
//     }
//     else {
//         perror("FORK FAILED.");
//     }
// }


// // execute parsered cmds
// void Execute() {
//     if (commandcount == 1 && command[0].cmdslen == 0) { // in order not to execute an empty command
//         ClearAll(1);
//         return;
//     }

//     int amp_flag = 0;

//     for (int i=0; i<commandcount; ++i) { // "&" handler
//         for (int j=0; j<command[i].cmdslen; ++j) {
//             if (command[i].cmds[j][0] == '&') {
//                 if ( ((i+1)== commandcount) && ((j+1)== command[i].cmdslen) ) {
//                     amp_flag = 1;
//                     free(command[i].cmds[j]);
//                     command[i].cmds[j] = NULL;
//                     --command[i].cmdslen;
//                 }
//                 else {
//                     printf("Error: bad positioning of \"&\", or more than one \"&\" in line.\n");fflush(stdout);
//                     return;
//                 }
//             }
//         }
//     }
//     for (int i=0; i<commandcount; ++i) {
//         if ( ((i+1)== commandcount) && (amp_flag) ) {
//             ExecuteSingleCommand(i,1);
//         }
//         else {
//             ExecuteSingleCommand(i,0);
//         }
//     }
//     ClearAll(1);
// }


// temporary function for debug
void Debug(void) {
    for (int i=0; i<grpcount; ++i) {
        printf("GROUP %d:\n", i);fflush(stdout);
        for (int j=0; j<grp[i].commandcount; ++j) {
            printf("     COMMAND %d:\n", j);fflush(stdout);
            for (int k=0; k<grp[i].command[j].cmdslen; ++k) {
                printf("          cmd[%d]:   %s\n", k, grp[i].command[j].cmds[k]);fflush(stdout);
            }
            printf("\n");fflush(stdout);
        }
    }
    ClearAll(1);
    // printf("state = %d\ncmdslen = %d\n", state, command[commandcount-1].cmdslen);fflush(stdout);
    // for (int i = 0; i < command[commandcount-1].cmdslen; ++i) {
    //     printf("Cmd %d:  %s\n", i, command[commandcount-1].cmds[i]);fflush(stdout);
    // }
}





int main() {
    int status, working;
    char ch;
    char cwd[1024] = "$";
    getcwd(cwd, sizeof(cwd)); printf("%s$ ", cwd);fflush(stdout); // вывод строки текущей папки

    ClearAll(1); // in fact, it's malloc(command) +zeroing its parts

    while (1) {
        // ch = getchar(); /*заменить на неблокирующий вариант getchar'а*/
        
        ch = read_nonblock();
        
        if (ch == 0) {
            usleep(10000);  // in order not to overload the CPU while infinite reading "nothing"
            continue;
        }
        if (ch == EOF) {
            ClearAll(0);
            printf("\nExit.\n");fflush(stdout);
            return 0;
        }
        if ( (ch == '\n') && (Is_ready2execute()) ) {
            // Execute();
            Debug();

            state = SPACE;

            /*здеся будет проверка всех фоновых процессов: for до prscount и waitpid каждому prs[i]*/
            working = 0;
            for (int i=0; i<prscount; ++i) {
                if ( (prs[i]) && (waitpid(prs[i], &status, WNOHANG) == prs[i]) ) {
                    printf("[%d] (%d) done with exit code: %d\n", i+1, prs[i], WEXITSTATUS(status));
                    prs[i] = 0;
                }
                else if (prs[i]) {
                    working = 1;
                }
            }
            if (!working) {
                free(prs);
                prs = NULL;
                prscount = 0;
            }
            getcwd(cwd, sizeof(cwd)); printf("%s$ ", cwd);fflush(stdout); // вывод строки текущей папки
        }
        else {
            Parser(ch);
        }
    }
}
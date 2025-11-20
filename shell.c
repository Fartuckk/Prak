#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>



int group_flag = 0,
    pipe_flag = 0,
    amp_flag = 0,
    stat = -1,
    global_stat = -1;



typedef union {
    int pipes[2];
    struct {
        int read;
        int write;
    };
} pipe2;

pipe2 *pipe_lst = NULL;

typedef struct {
    char** cmds;
    int cmdslen;
} cmnd;

typedef struct {
    cmnd* command;
    int commandcount;
} group;

group* grp = NULL;
int grpcount = 0;



typedef struct {
    int* prs;           // list of background processes, contains their pids
    int prscount;
} prs_lst;

prs_lst prs_list[2] = {{.prs = NULL, .prscount = 0},{.prs = NULL, .prscount = 0}};



enum {
    SPACE,
    REGULAR,
    QUOTE,
    PIPE,
    AMP
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



// restore blocking mode for stdin
void restore_blocking(void) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

// is equivalent to getchar(), but is non-blocking.
// returns char if read successfully, 0 if nothing was read., -1 if EOF was read.
int read_nonblock() { /*при необходимости можно заменить на read_nonblock(char *c), для корректизации возвращаемых значений*/
    unsigned char c;
    int res;
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    res = read(STDIN_FILENO, &c, sizeof(char));

    restore_blocking();
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

// close all descriptors but for rd_fd for read and wrt_fd for write (in group[k])
void CloseAll(int rd_fd, int wrt_fd, int k) {
    // printf("%d\n", grp[k].commandcount-1);
    for (int n=0; n<grp[k].commandcount-1; ++n) {
        // printf("n: %d, rd: %d, wrt: %d\n", n, rd_fd, wrt_fd);fflush(stdout);
        // printf("%p, %d\n", &pipe_lst[n], rd_fd);fflush(stdout);
        if (pipe_lst[n].read != rd_fd) {
            close(pipe_lst[n].read);
            // printf("closed read: %d\n", pipe_lst[n].read);fflush(stdout);
        }
        else {
            // printf("kept read: %d\n", pipe_lst[n].read);fflush(stdout);
        }
        if (pipe_lst[n].write != wrt_fd) {
            close(pipe_lst[n].write);
            // printf("closed write: %d\n", pipe_lst[n].write);fflush(stdout);
        }
        else {
            // printf("kept write: %d\n", pipe_lst[n].write);fflush(stdout);
        }
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
    return ( (ch == '=') || (ch == '>') || (ch == '<') || (ch == '!') || (ch == '$') || (ch == '^') || (ch == ':') || (ch == ',') );
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

// returns 1 if char is "pipe", otherwise returns 0
int Is_amp(char ch) {
    return (ch == '&');
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
            else if ( Is_amp(ch) ) {
                Put2NewWord(ch);
                state = AMP;
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
            else if ( Is_amp(ch) ) {
                Put2NewWord(ch);
                state = AMP;
            }
            else { // is regular.
                Put2CurWord(ch);
            }
        break;

        case PIPE:
            if ( Is_space(ch) ) {
                pipe_flag = 1;
            }
            else if ( Is_special(ch) ) {
                printf("Parse error.\n");fflush(stdout);
                exit(1);
            }
            else if ( Is_quote(ch) ) {
                pipe_flag = 0;
                CreateNewCommand();
                Put2NewWord(ch);
                state = QUOTE;
            }
            else if ( Is_semicolon(ch) ) {
                printf("Parse error.\n");fflush(stdout);
                exit(1);
            }
            else if ( Is_pipe(ch) ) {
                if (pipe_flag) {
                    printf("Parse error.\n");fflush(stdout);
                    exit(1);
                }
                else {
                    // printf("Parse error.\n");fflush(stdout);
                    // exit(1);
                    CreateNewGroup();
                    Put2NewWord('|');
                    Put2CurWord('|');
                    CreateNewGroup();
                    state = SPACE;
                    /* "||" logical operator */
                }
            }
            else if ( Is_amp(ch) ) {
                printf("Parse error.\n");fflush(stdout);
                exit(1);
            }
            else { // is regular
                pipe_flag = 0;
                CreateNewCommand();
                Put2NewWord(ch);
                state = REGULAR;
            }
        break;

        case AMP:
            if ( Is_space(ch) ) {
                amp_flag = 1;
            }
            else if ( Is_amp(ch) ) {
                if (amp_flag) {
                    printf("Parse error.\n");fflush(stdout);
                    exit(1);
                }
                else {
                    // printf("Parse error.\n");fflush(stdout);
                    // exit(1);
                    int k = grpcount-1,
                        m = grp[k].commandcount-1,
                        n = grp[k].command[m].cmdslen-1;
                    free(grp[k].command[m].cmds[n]);
                    grp[k].command[m].cmds[n] = NULL;
                    --grp[k].command[m].cmdslen;
                    CreateNewGroup();
                    Put2NewWord('&');
                    Put2CurWord('&');
                    CreateNewGroup();
                    state = SPACE;
                    /* "&&" logical operator: need to replace '&' in last word with '\0' */
                }
            }
            else { // is regular
                printf("Parse error.\n");fflush(stdout);
                exit(1);
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




// execute command[l] from group[k], waiting till its completion if bckgrnd_mode = 0, executing it at background while adding its pid to the prs* if bckgrnd_mode = 1
int ExecuteSingleCommand(int k, int l, int bckgrnd_mode, int read_fd, int write_fd, int prs_list_number) {
    int len = grp[k].command[l].cmdslen;
    grp[k].command[l].cmds = realloc(grp[k].command[l].cmds, (len + 1)*sizeof(char *));
    grp[k].command[l].cmds[len] = NULL;

    int cur = 1;
    if (!strcmp(grp[k].command[l].cmds[0], "cd")) {     // "cd" command handler
        while ( (grp[k].command[l].cmds[cur] != NULL) && (grp[k].command[l].cmds[cur][0] == '-') ) {  // parameters for "cd" handler
            if ( Check_cd_parameter(grp[k].command[l].cmds[cur]) == 0 ) {
                printf("cd: invalid option\n");fflush(stdout);
                stat = 1;
                return 0;
            }
            ++cur;
        }

        if (chdir(grp[k].command[l].cmds[cur]) == -1) {
            if (grp[k].command[l].cmds[cur] == NULL) {
                return 0;
            }
            perror("cd failed");
            stat = 1;
        }
        return 0;
    }

    pid_t pid = fork();
    int status;
    char *s;
    if (pid == 0) {                              // SON
        // printf("command: %s,     read_fd/write_fd:    %d / %d\n", grp[k].command[l].cmds[0], read_fd, write_fd);
        CloseAll(read_fd, write_fd, k);
        if (read_fd != STDIN_FILENO) {
            dup2(read_fd, STDIN_FILENO);
            // close(read_fd);
        }
        if (write_fd != STDOUT_FILENO) {
            dup2(write_fd, STDOUT_FILENO);
            // close(write_fd);
        }
        execvp(grp[k].command[l].cmds[0], grp[k].command[l].cmds);
        // we are here only if execvp exited with error
        s = grp[k].command[l].cmds[0];
        perror(s);
        exit(1);
    }
    else if (pid > 0) {                        // FATHER
        if (bckgrnd_mode) {
            prs_list[prs_list_number].prs = realloc(prs_list[prs_list_number].prs, (prs_list[prs_list_number].prscount+1)*sizeof(int *));
            prs_list[prs_list_number].prs[prs_list[prs_list_number].prscount] = pid;
            ++prs_list[prs_list_number].prscount;
            // if (write_fd == STDOUT_FILENO) {
            //     printf("[%d] %d\n", prs_list[prs_list_number].prscount, pid);fflush(stdout);
            // }
            return pid;
        }
        else {
            if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
            }
            stat = !!WEXITSTATUS(status);
        }
    }
    else {
        perror("FORK FAILED.");
    }
}

// execute group[k]
void ExecuteGroup(int k) {
    if ((grp[k].commandcount == 1) && (grp[k].command[0].cmdslen == 0)) {
        global_stat = 0;
        return;
    }

    int background_flag = 0,
        m = grp[k].commandcount-1,
        n = grp[k].command[m].cmdslen-1;

    if (grp[k].command[m].cmds[n][0] == '&') {
        background_flag = 1;
        free(grp[k].command[m].cmds[n]);
        grp[k].command[m].cmds[n] = NULL;
        --grp[k].command[m].cmdslen;
    }
    
    pipe_lst = malloc(sizeof(pipe2)*(grp[k].commandcount-1));

    for (int i=0; i<grp[k].commandcount-1; ++i) {
        pipe(pipe_lst[i].pipes);
    }
    if (grp[k].commandcount == 1) {
        if (background_flag) {
            int pid = ExecuteSingleCommand(k, 0, background_flag, STDIN_FILENO, STDOUT_FILENO, 0);
            printf("[%d] %d\n", prs_list[0].prscount, pid);fflush(stdout);
        }
        else {
            ExecuteSingleCommand(k, 0, background_flag, STDIN_FILENO, STDOUT_FILENO, 0);
            global_stat = stat;
            // printf("**%d\n", global_stat);
        }
    }
    else {
        if (background_flag) {
            int pid = ExecuteSingleCommand(k, grp[k].commandcount-1, 1, pipe_lst[grp[k].commandcount-2].read, STDOUT_FILENO, 0);
            for (int i=grp[k].commandcount-2; i>0; --i) {
                ExecuteSingleCommand(k, i, 1, pipe_lst[i-1].read, pipe_lst[i].write, 0);
            }
            ExecuteSingleCommand(k, 0, 1, STDIN_FILENO, pipe_lst[0].write, 0);
            CloseAll(-1,-1,k);
            printf("[1] %d\n", pid);fflush(stdout);
        }
        else {
            ExecuteSingleCommand(k, grp[k].commandcount-1, 1, pipe_lst[grp[k].commandcount-2].read, STDOUT_FILENO, 1);
            global_stat = stat;
            for (int i=grp[k].commandcount-2; i>0; --i) {
                ExecuteSingleCommand(k, i, 1, pipe_lst[i-1].read, pipe_lst[i].write, 1);
            }
            ExecuteSingleCommand(k, 0, 1, STDIN_FILENO, pipe_lst[0].write, 1);
            // sleep(1);
            CloseAll(-1,-1,k);

            if (prs_list[1].prscount > 0) {
                int status;
                waitpid(prs_list[1].prs[0], &status, 0);
                global_stat = !!WEXITSTATUS(status);
            }
            for (int j=1; j<prs_list[1].prscount; ++j) {
                // printf("%d ***** %d\n", j, prs_list[1].prs[j]);
                waitpid(prs_list[1].prs[j], NULL, 0);
            }

            if (prs_list[1].prs) {
                free(prs_list[1].prs);
                prs_list[1].prs = NULL;
                prs_list[1].prscount = 0;
            }
        }
    }
    // for (int i=0; i<grp[k].commandcount-1; ++i) {
    //     printf("pipe[%d]   read:  %d\n", i, pipe_lst[i].read);
    //     printf("pipe[%d]  write:  %d\n", i, pipe_lst[i].write);
    // }
}

// execute all groups
void Execute() {
    int s;
    for (int i=0; i<grpcount; ++i) {
        if ((grp[i].commandcount == 1) && (grp[i].command[0].cmdslen == 1) && (!strcmp(grp[i].command[0].cmds[0], "&&"))) {
            // printf("%d\n", global_stat);
            if (global_stat == 1) {
                ClearAll(1);
                return;
            }
            continue;
        }
        if ((grp[i].commandcount == 1) && (grp[i].command[0].cmdslen == 1) && (!strcmp(grp[i].command[0].cmds[0], "||"))) {
            // printf("%d\n", global_stat);fflush(stdout);
            if (global_stat == 0) {
                ClearAll(1);
                return;
            }
            continue;
        }
        ExecuteGroup(i);
    }
    ClearAll(1);
}

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
    getcwd(cwd, sizeof(cwd)); printf("%s$ ", cwd);fflush(stdout); // prompt output

    ClearAll(1); // in fact, it's malloc(command) +zeroing its parts

    while (1) {
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
            Execute();
            // Debug();

            state = SPACE;

            /* here is the check of all background processes: for till prscount and waitpid each prs[i] */
            working = 0;
            for (int i=0; i<prs_list[0].prscount; ++i) {
                if ( (prs_list[0].prs[i]) && (waitpid(prs_list[0].prs[i], &status, WNOHANG) == prs_list[0].prs[i]) ) {
                    printf("[%d] (%d) done with exit code: %d\n", i+1, prs_list[0].prs[i], WEXITSTATUS(status));
                    prs_list[0].prs[i] = 0;
                }
                else if (prs_list[0].prs[i]) {
                    working = 1;
                }
            }
            if (!working) {
                free(prs_list[0].prs);
                prs_list[0].prs = NULL;
                prs_list[0].prscount = 0;
            }
            getcwd(cwd, sizeof(cwd)); printf("%s$ ", cwd);fflush(stdout); // prompt output
        }
        else {
            Parser(ch);
        }
    }
}
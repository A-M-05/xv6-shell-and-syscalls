// apmanu

// This is a shell that parses the command line, similar to xv6's sh.c
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXLINE 512
#define MAXARGS 32

// -------------------- helpers: command classification --------------------

int should_fallback(char *cmd){
    /*
    Checks if a command can fallback to the parent directory or not,
    and it does this by checking if there is a path present or not.
    If there is an explicit path, no fallback necessary.
    If there is no explicit path, it is an identifier and
    will signal to fallback to the parent directory.
    Arguments: 
        char *cmd
            the pointer to where the path/identifier starts
    Return: 
        int
            0 if it is a path (no fallback)
            1 if it is an identifier (fallback)
    */
    
    if(cmd == 0 || cmd[0] == 0){
        return 0;
    }

    // if it starts with '.', its a relative path like ./prog or ../prog, so no fallback
    if(cmd[0] == '.'){
        return 0;
    }

    // if it contains '/', it's some path, so no fallback
    for(int i = 0; cmd[i] != 0; ++i){
        if(cmd[i] == '/'){
            return 0;
        }
    }

    // otherwise, its a simple identifier, so it's okay to fallback
    return 1;
}

// -------------------- helpers: line / parsing --------------------

int is_blank_line(char *s){
    /*
    Checks if a line is blank, and it does
    this by checking if s is the terminator or whitespace.
    Arguments:
        char *s
            pointer to the beginning of the line
    Return:
        int
            0 if not a blank line
            1 if a blank line
    */

    if(s == 0) return 1;
    while(*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    return *s == 0;
}


int is_comment_line(char *s){
    /*
    Checks if the line is a comment line,
    and it does this by checking if the first 
    character is a hashtag/octothorp sign.
    Arguments:
        char *s
            pointer to the beginning of the line
    Return:
        int
            0 if not a comment line
            1 if a comment line
    */

    if(s == 0) return 0;
    while(*s == ' ' || *s == '\t')
        s++;
    return *s == '#';
}

int readline(int fd, char *buf, int max){
    int i = 0;
    while(i + 1 < max){
        char c;
        int n = read(fd, &c, 1);
        if(n < 1){
            break;      // error with read
        }

        if(c == '\n'){
            buf[i] = 0;
            return 1;   // got full line
        }

        buf[i++] = c;
    }
    buf[i] = 0;
    if(i == 0){
        return 0;       // no data, EOF
    }
    return 1;           // last line without trailing newline
}

int count_pipes(int argc, char *argv[]){
    /*
    Counts the number of '|' tokens in the line.
    Arguments:
        int argc
            the length of the line
        char *argv[]
            the character-string line
    Return:
        int
            number of '|' tokens
    */

    int pipes = 0;
    for(int i = 0; i < argc; ++i){
        if(strcmp(argv[i], "|") == 0)
            pipes++;
    }
    return pipes;
}

// Tokenizes args and returns argc, argv updated in-place.
int parse_args(char *args, char *argv[], int maxargs){
    char *cmd = args;
    int argc = 0;

    // skip leading spaces and tabs and newlines
    while(*cmd == ' ' || *cmd == '\t' || *cmd == '\n'){
        cmd++;
    }
    
    // tokenize
    while(*cmd != 0 && argc < maxargs-1){
        // start of token
        argv[argc] = cmd;
        argc++;

        // advance until delimiter or end
        while(*cmd != 0 && *cmd != ' ' && *cmd != '\t' && *cmd != '\n'){
            cmd++;
        }

        if(*cmd == ' ' || *cmd == '\t' || *cmd == '\n'){
            *cmd = 0;   // end the word
            cmd++;      // move past NULL
            // skip more spaces and tabs
            while(*cmd == ' ' || *cmd == '\t' || *cmd == '\n'){
                cmd++;
            }
        }
    }

    argv[argc] = 0;
    return argc;
}

// -------------------- helpers: redirection --------------------

int redirection_logic(int argc, char *argv[], char **infile, char **outfile, char **errfile){
    for(int i = 0; i < argc; ++i){
        if(strcmp(argv[i], "<") == 0){
            if(i + 1 >= argc){
                fprintf(2, "error\n");
                return -1;
            }
            *infile = argv[i+1];
            for(int j = i; j + 2 <= argc; ++j){
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            i--;
        }
        else if(strcmp(argv[i], ">") == 0){
            if(i + 1 >= argc){
                fprintf(2, "error\n");
                return -1;
            }
            *outfile = argv[i+1];
            for(int j = i; j + 2 <= argc; ++j){
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            i--;
        }
        else if(strcmp(argv[i], "!") == 0){
            if(i + 1 >= argc){
                fprintf(2, "error\n");
                return -1;
            }
            *errfile = argv[i+1];
            for(int j = i; j + 2 <= argc; ++j){
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            i--;
        }
    }
    return argc;
}

// Apply <, >, ! redirections in a child process.
// Returns 0 on success, -1 on error (and prints error).
int apply_redirections(char *infile, char *outfile, char *errfile){
    if(infile != 0){
        close(0);
        int infile_check = open(infile, O_RDONLY);
        if(infile_check < 0){
            if(outfile != 0){
                unlink(outfile);
            }
            if(errfile != 0){
                unlink(errfile);
            }
            fprintf(2, "error\n");
            return -1;
        }
    }
    if(outfile != 0){
        close(1);
        int outfile_check = open(outfile, O_WRONLY | O_CREATE | O_TRUNC);
        if(outfile_check < 0){
            fprintf(2, "error\n");
            return -1;
        }
    }
    if(errfile != 0){
        close(2);
        int errfile_check = open(errfile, O_WRONLY | O_CREATE | O_TRUNC);
        if(errfile_check < 0){
            fprintf(2, "error\n");
            return -1;
        }
    }
    return 0;
}

// -------------------- pipeline execution --------------------

void run_pipeline(int argc, char *argv[], int num_pipes){
    // parse through argv, finding pipes
    char **cmd_argvs[MAXARGS];
    int num_cmds = num_pipes + 1;
    int pids[MAXARGS];
    int alive[MAXARGS];

    for(int i = 0; i < MAXARGS; ++i){
        pids[i] = -1;
        alive[i] = 0;
    }

    cmd_argvs[0] = &argv[0];
    int cmd_index = 0;

    for(int i = 0; i < argc; ++i){
        if(strcmp(argv[i], "|") == 0){
            argv[i] = 0;
            cmd_index++;
            cmd_argvs[cmd_index] = &argv[i+1];
        }
    }

    int prev_pipe = -1;

    for(int i = 0; i < num_cmds; ++i){
        char *infile = 0;
        char *outfile = 0;
        char *errfile = 0;
        int p[2];

        if(i != num_cmds - 1){
            // create pipe for this output
            int res = pipe(p);
            if(res < 0){
                fprintf(2, "error\n");
                return;
            }
        }

        int pid = fork();
        if(pid < 0){
            fprintf(2, "error\n");
            return;
        }
        else if(pid == 0){
        // child

        if(prev_pipe != -1){
            close(0);
            dup(prev_pipe);
            close(prev_pipe);
        }

        if(i != num_cmds - 1){
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
        }

        int cmd_argc = 0;
        while (cmd_argvs[i][cmd_argc] != 0) cmd_argc++;

        int new_cmd_argc = redirection_logic(cmd_argc, cmd_argvs[i], &infile, &outfile, &errfile);
        if(new_cmd_argc < 0){
            if(outfile != 0){
                unlink(outfile);
            }
            if(errfile != 0){
                unlink(errfile);
            }
            fprintf(2, "error\n");
            exit(1);
        }
        cmd_argc = new_cmd_argc;
        cmd_argvs[i][cmd_argc] = 0;

        if(apply_redirections(infile, outfile, errfile) < 0){
            exit(-1);
        }

        // builtin about inside pipeline
        if(strcmp(cmd_argvs[i][0], "about") == 0){
            if (cmd_argc == 1) {
                fprintf(1, "MyShell:\n");
                fprintf(1, "\tThis shell is similar to the xv6 shell,\n");
                fprintf(1, "\tbut it has some extra capabilities.\n");
                fprintf(1, "\tPrepared to be amazed!\n");
                fprintf(1, "Author: Abraham Manu\n");
            }
            else {
                fprintf(1, "my about message\n");
            }
            exit(0);
        }

        exec(cmd_argvs[i][0], cmd_argvs[i]);

        if(should_fallback(cmd_argvs[i][0])){
            int len = strlen(cmd_argvs[i][0]);
            char buffer[len + 2];
            buffer[0] = '/';
            strcpy(buffer + 1, cmd_argvs[i][0]);
            exec(buffer, cmd_argvs[i]);
        }

        if(outfile != 0){
            unlink(outfile);
        }
        if(errfile != 0){
            unlink(errfile);
        }
        fprintf(2, "error\n");
        exit(-1);
        }
        else{
            // parent
            pids[i] = pid;
            alive[i] = 1;

            if(prev_pipe != -1){
                close(prev_pipe);
            }

            if(i != num_cmds - 1){
                close(p[1]);
                prev_pipe = p[0];
            }
            else{
                prev_pipe = -1;
            }
        }
    }

    if(prev_pipe != -1){
        close(prev_pipe);
    }

    int remaining = num_cmds;

    while(remaining > 0){
        int status;
        int wpid = wait(&status);
        if(wpid < 0){
            break;
        }

        int idx = -1;
        for(int j = 0; j < num_cmds; ++j){
            if(pids[j] == wpid){
                idx = j;
                break;
            }
        }

        if (idx >= 0 && alive[idx]) {
            alive[idx] = 0;
            remaining--;
        }

        if(status != 0){
            for(int j = 0; j < num_cmds; ++j){
                if(alive[j] && pids[j] > 0){
                    kill(pids[j]);
                    alive[j] = 0;
                }
            }
        }
    }
}

// -------------------- single-command execution --------------------

void run_command(int argc, char *argv[]){
    char *infile = 0;
    char *outfile = 0;
    char *errfile = 0;
    int new_argc = redirection_logic(argc, argv, &infile, &outfile, &errfile);
    if(new_argc < 0){
        return;
    }
    argc = new_argc;
    argv[argc] = 0;

    int pid = fork();
    
    if(pid < 0){
        fprintf(2, "error\n");
    }
    else if(pid == 0){
        if(apply_redirections(infile, outfile, errfile) < 0){
            exit(-1);
        }

        exec(argv[0], argv);

        if(should_fallback(argv[0])){
            int len = strlen(argv[0]);
            char buffer[len + 2];
            buffer[0] = '/';
            strcpy(buffer + 1, argv[0]);
            exec(buffer, argv);
        }

        if(outfile != 0){
            unlink(outfile);
        }
        if(errfile != 0){
            unlink(errfile);
        }
        fprintf(2, "error\n");
        exit(-1);
    }
    else{
        wait(0);
    }
}

// -------------------- builtins --------------------

void handle_cd(int argc, char *argv[]){
    char *target;
    if(argc < 2){
        target = "/";
    }
    else {
        target = argv[1];
    }
    if(chdir(target) < 0){
        fprintf(2, "error\n");
    }
}

// parse exit status argument, return value or -1 on invalid
int parse_exit_status(int argc, char *argv[]){
    if (argc < 2) return 0;

    char *s = argv[1];
    int sign = 1;
    int i = 0;

    if (s[0] == '-') {
        sign = -1;
        i = 1;
    }

    if (s[i] == '\0') {
        return -1;
    }

    int value = 0;
    int valid = 1;

    for (; s[i] != '\0'; i++){
        if (s[i] >= '0' && s[i] <= '9'){
            value = value * 10 + (s[i] - '0');
        }
        else{
            valid = 0;
            break;
        }
    }

    if (!valid) {
        return -1;
    }

    value = sign * value;
    if (value < -128 || value > 127) {
        return -1;
    }

    return value;
}

void handle_exit(int argc, char *argv[]){
    int status = parse_exit_status(argc, argv);
    fprintf(1, "bye\n");
    exit(status);
}

void handle_about_builtin(int argc, char *argv[]){
    char *infile = 0;
    char *outfile = 0;
    char *errfile = 0;

    int new_argc = redirection_logic(argc, argv, &infile, &outfile, &errfile);
    if(new_argc < 0){
        return;
    }

    argc = new_argc;
    (void) infile; // ignored

    int saved_stdout = -1;
    int saved_stderr = -1;

    if(outfile != 0){
        saved_stdout = dup(1);
        if(saved_stdout < 0){
            fprintf(2, "error\n");
            return;
        }

        close(1);
        int fd = open(outfile, O_WRONLY | O_CREATE | O_TRUNC);
        if(fd < 0){
            fprintf(2, "error\n");
            dup(saved_stdout);
            close(saved_stdout);
            return;
        }
    }
    if(errfile != 0){
        saved_stderr = dup(2);
        if(saved_stderr < 0){
            fprintf(2, "error\n");
            if(saved_stdout >= 0) {
                close(1);
                dup(saved_stdout);
                close(saved_stdout);
            }
            return;
        }

        close(2);
        int fd = open(errfile, O_WRONLY | O_CREATE | O_TRUNC);
        if(fd < 0){
            fprintf(2, "error\n");
            dup(saved_stderr);
            close(saved_stderr);
            if(saved_stdout >= 0) {
                close(1);
                dup(saved_stdout);
                close(saved_stdout);
            }
            return;
        }
    }

    if(argc == 1){
        fprintf(1, "MyShell:\n");
        fprintf(1, "\tThis shell is similar to the xv6 shell,\n");
        fprintf(1, "\tbut it has some extra capabilities.\n");
        fprintf(1, "\tPrepared to be amazed!\n");
        fprintf(1, "Author: Abraham Manu\n");
    }
    else{
        fprintf(1, "my about message\n");
    }

    if(saved_stderr >= 0) {
        close(2);
        dup(saved_stderr);
        close(saved_stderr);
    }
    if(saved_stdout >= 0) {
        close(1);
        dup(saved_stdout);
        close(saved_stdout);
    }
}

// -------------------- command dispatch + shell loop --------------------

void dispatch_command(int argc, char *argv[]){
    if(argc == 0)
        return;

    int pipe_count = count_pipes(argc, argv);

    if(pipe_count > 0){
        run_pipeline(argc, argv, pipe_count);
        return;
    }

    if(strcmp(argv[0], "cd") == 0){
        handle_cd(argc, argv);
    }
    else if(strcmp(argv[0], "exit") == 0){
        handle_exit(argc, argv);
    }
    else if(strcmp(argv[0], "about") == 0){
        handle_about_builtin(argc, argv);
    }
    else{
        run_command(argc, argv);
    }
}

void shell_loop(int fd, int script_mode){
    char args[MAXLINE];
    char *commands[MAXARGS];

    while(1){
        if(!script_mode){
            fprintf(2, "cs143a$ ");
        }
        
        if(script_mode){
            if(readline(fd, args, sizeof(args)) == 0){
                // EOF, no bye for script mode
                exit(0);
            }
        }
        else{
            if(gets(args, sizeof(args)) == 0){
                // EOF
                fprintf(1, "\nbye\n");
                exit(0);
            }
        }

        if(is_blank_line(args)){
            continue;
        }

        if(is_comment_line(args)){
            continue;
        }

        int num_commands = parse_args(args, commands, MAXARGS);
        if(script_mode && num_commands == 0){
            continue;
        }

        dispatch_command(num_commands, commands);
    }
}

// -------------------- main --------------------

int main(int argc, char *argv[]){
    int script_mode = 0;
    int fd = 0;
    
    if(argc == 2){
        script_mode = 1;
        fd = open(argv[1], O_RDONLY);
        if(fd < 0){
            fprintf(2, "error, cannot open that");
            exit(1);
        }
    }
    else if(argc > 2){
        fprintf(2, "usage: myshell [script]\n");
        exit(1);
    }

    shell_loop(fd, script_mode);
    return 0;
}

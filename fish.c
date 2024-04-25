#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <stdlib.h>

#include "cmdline.h"

#define BUFLEN 512

#define YES_NO(i) ((i) ? "Y" : "N")

/**
 * Executes a command
 * @param commands The command to execute
 */
void execute_command(struct cmd *commands) {
    pid_t pid = fork();
    if (pid == 0) {
        // Execute the command
        execvp(commands->args[0], commands->args);
        perror("execvp failed");
    }

    int stat;
    wait(&stat);

    // Print child end status
    if (WIFEXITED(stat)) {
        fprintf(stderr, "Command finished with exit status %i\n", WEXITSTATUS(stat));
    }
    if (WIFSIGNALED(stat)) {
        fprintf(stderr, "Command finished with signal %i\n", WTERMSIG(stat));
    }
}

/**
 * Change the current working directory
 * @param path The path to set the current working directory to
 */
void cd(char *path) {
    if (strcmp(path, "~") == 0) {

    }
}

/**
 * Process a command line by executing all its commands
 * @param line The line to process
 */
void execute_line(struct line *line) {
    for (size_t i = 0; i < line->n_cmds; ++i) {
        // Execute the cd command
        if (line->cmds[i].n_args == 1 && strcmp(line->cmds[i].args[0], "cd") == 0) {
            cd(line->cmds[i].args[0]);
        }
        else execute_command(&line->cmds[i]);
    }
}

int main() {
    struct line li;
    char buf[BUFLEN];

    line_init(&li);

    for (;;) {
        char *buffer = calloc(BUFSIZ, sizeof(char));
        char *cwd = getcwd(buffer, BUFSIZ);
        printf("%s fish> ", cwd != NULL ? cwd : "");
        free(buffer);

        fgets(buf, BUFLEN, stdin);

        int err = line_parse(&li, buf);
        if (err) {
            //the command line entered by the user isn't valid
            line_reset(&li);
            continue;
        }

        fprintf(stderr, "Command line:\n");
        fprintf(stderr, "\tNumber of commands: %zu\n", li.n_cmds);

        for (size_t i = 0; i < li.n_cmds; ++i) {
            fprintf(stderr, "\t\tCommand #%zu:\n", i);
            fprintf(stderr, "\t\t\tNumber of args: %zu\n", li.cmds[i].n_args);
            fprintf(stderr, "\t\t\tArgs:");
            for (size_t j = 0; j < li.cmds[i].n_args; ++j) {
                fprintf(stderr, " \"%s\"", li.cmds[i].args[j]);
            }
            fprintf(stderr, "\n");
        }

        fprintf(stderr, "\tRedirection of input: %s\n", YES_NO(li.file_input));
        if (li.file_input) {
            fprintf(stderr, "\t\tFilename: '%s'\n", li.file_input);
        }

        fprintf(stderr, "\tRedirection of output: %s\n", YES_NO(li.file_output));
        if (li.file_output) {
            fprintf(stderr, "\t\tFilename: '%s'\n", li.file_output);
            fprintf(stderr, "\t\tMode: %s\n", li.file_output_append ? "APPEND" : "TRUNC");
        }

        fprintf(stderr, "\tBackground: %s\n", YES_NO(li.background));

        if (li.cmds[0].n_args == 1 && strcmp(li.cmds[0].args[0], "exit") == 0) break;

        execute_line(&li);

        line_reset(&li);
    }

    return 0;
}

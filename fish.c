#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "cmdline.h"

#define BUFLEN 512

#define YES_NO(i) ((i) ? "Y" : "N")

/**
 * Executes a command
 * @param commands The command to execute
 */
void execute_command(struct cmd *commands, char *file_input, char *file_output, bool file_output_append) {
    pid_t pid = fork();

    if (pid == 0) {
        // Redirecting input
        if (file_input != NULL) {
            int input = open(file_input, O_RDONLY);
            if (input == -1) {
                perror("Input redirection failed");
            }
            else {
                dup2(input, STDIN_FILENO);
                close(input);
            }
        }

        // Redirecting output
        if (file_output != NULL) {
            int output = open(
                    file_output,
                    O_WRONLY | O_CREAT | (file_output_append ? O_APPEND : O_TRUNC)
            );
            if (output == -1) perror("Output redirection failed");
            else {
                dup2(output, STDOUT_FILENO);
                close(output);
            }
        }

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
    char *newPath = NULL;
    if (strcmp(path, "~") == 0) {
        newPath = getenv("HOME");
        if (newPath == NULL) {
            fprintf(stderr, "Error while reading the HOME environment variable");
            return;
        }
    }

    int status = chdir(newPath != NULL ? newPath : path);
    if (status == -1) perror("Failed to set working directory");
    else if (status != 0) {
        fprintf(stderr, "Failed to set working directory with error %d", status);
    }
}

/**
 * Process a command line by executing all its commands
 * @param line The line to process
 */
void execute_line(struct line *line) {
    for (size_t i = 0; i < line->n_cmds; ++i) {
        // Execute the cd command
        if (line->cmds[i].n_args == 2 && strcmp(line->cmds[i].args[0], "cd") == 0) {
            cd(line->cmds[i].args[1]);
        }
        // Execute other commands
        else execute_command(
                &line->cmds[i],
                i == 0 ? line->file_input : NULL,
                i == line->n_cmds - 1 ? line->file_output : NULL,
                line->file_output_append
            );
    }
}

int main() {
    struct line li;
    char buf[BUFLEN];

    line_init(&li);

    for (;;) {
        char *cwd = getcwd(NULL, 0);
        printf("%s fish> ", cwd != NULL ? cwd : "");
        if (cwd != NULL) free(cwd);

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

        // Handle the exit command
        if (
                li.n_cmds == 1
                && li.cmds[0].n_args == 1
                && strcmp(li.cmds[0].args[0], "exit") == 0
        ) {
            line_reset(&li);
            return 0;
        }

        execute_line(&li);

        line_reset(&li);
    }
}

#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libgen.h>

#include "cmdline.h"

#define BUFLEN 512
#define ENDSTATUS_BUF_LEN 1024

#define YES_NO(i) ((i) ? "Y" : "N")

char *endstatus;

/**
 * Reads the termination state of a process and displays how it ended
 * @param pid The pid of the process to read the state from
 */
void read_process_state(pid_t pid) {
    int stat;
    pid = waitpid(pid, &stat, 0);
    if (pid == -1) {
        perror("waitpid");
        return;
    }

    // Print child end status
    if (WIFEXITED(stat)) {
        snprintf(
                endstatus, ENDSTATUS_BUF_LEN,
                "PID %d finished with exit status %i\n", pid, WEXITSTATUS(stat)
        );
    }
    if (WIFSIGNALED(stat)) {
        snprintf(
                endstatus, ENDSTATUS_BUF_LEN,
                "PID %d finished with signal %i\n", pid, WTERMSIG(stat)
        );
    }
}

/**
 * An empty handler for SIGINT
 */
void sigint_handler() {}

/**
 * The handler for SIGCHLD
 */
void sigchld_handler() {
    read_process_state(-1);
}

/**
 * Executes a command
 * @param command The command to execute
 */
void execute_command(struct line *line, struct cmd *command, size_t commandIndex) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        if (line->background) {
            // Installing SIGINT signal handler
            struct sigaction action;
            action.sa_flags = 0;
            sigemptyset(&action.sa_mask);
            action.sa_handler = SIG_IGN;
            sigaction(SIGCHLD, &action, NULL);
        }

        // Redirecting input
        if ((commandIndex == 0 && line->file_input != NULL) || line->background) {
            int input = open(
                    line->file_input != NULL ? line->file_input : "/dev/null",
                    O_RDONLY
            );
            if (input == -1) perror("Input redirection failed");
            else {
                dup2(input, STDIN_FILENO);
                close(input);
            }
        }

        // Redirecting output
        if (commandIndex == line->n_cmds - 1 && line->file_output != NULL) {
            int output = open(
                    line->file_output,
                    O_WRONLY | O_CREAT | (line->file_output_append ? O_APPEND : O_TRUNC)
            );
            if (output == -1) perror("Output redirection failed");
            else {
                dup2(output, STDOUT_FILENO);
                close(output);
            }
        }

        // Execute the command
        execvp(command->args[0], command->args);
        perror("execvp failed");
        exit(1);
    }

    if (!line->background) pause();
}

/**
 * Change the current working directory
 * @param path The path to set the current working directory to
 */
void cd(char *path) {
    char *newPath = NULL;

    // Get the home path
    if (strcmp(path, "~") == 0) {
        newPath = getenv("HOME");
        if (newPath == NULL) {
            fprintf(stderr, "Error while reading the HOME environment variable");
            return;
        }
    }

    // Set the new current working directory
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
                line,
                &line->cmds[i],
                i
        );
    }
}

int main() {
    // Create buffer for displaying end status
    endstatus = calloc(ENDSTATUS_BUF_LEN, sizeof(char));

    // Install SIGINT signal handler
    struct sigaction action;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    action.sa_handler = sigint_handler;
    sigaction(SIGINT, &action, NULL);

    // Installing SIGCHLD signal handler
    struct sigaction act2;
    act2.sa_flags = SA_RESTART;
    sigemptyset(&act2.sa_mask);
    act2.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &act2, NULL);

    struct line li;
    char buf[BUFLEN];

    line_init(&li);


    for (;;) {
        // Display end status
        if (strlen(endstatus) > 0) {
            fprintf(stderr, "%s", endstatus);
            for (int i = 0; i < ENDSTATUS_BUF_LEN; ++i) endstatus[i] = '\0';
        }

        // Display prompt
        char *cwd = getcwd(NULL, 0);
        printf("fish %s> ", cwd != NULL ? basename(cwd) : "");
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
            free(endstatus);
            line_reset(&li);
            return 0;
        }

        execute_line(&li);

        line_reset(&li);
    }
}

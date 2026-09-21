#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include "../include/executor.h"
#include "../include/builtin.h"

extern int last_exit_status; /* defined in main.c */
int shell_exit_requested = 0;

/* ---------- MILESTONE-4.2: SIGCHLD handler (zombie prevention) ---------- */

static void sigchld_handler(int sig) {
    int saved_errno = errno;
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        /* Reap finished children */
    }
    errno = saved_errno;
}

void setup_background_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

static int run_external(command_t *cmd) {
    pid_t pid = fork();

    if (pid == 0) {
        if (cmd->background) {
            int null_fd = open("/dev/null", O_RDONLY);
            if (null_fd >= 0) {
                dup2(null_fd, STDIN_FILENO);
                close(null_fd);
            }
        }
        if (cmd->input[0]) {
            int fd_in = open(cmd->input, O_RDONLY);
            if (fd_in < 0) { perror(cmd->input); _exit(127); }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if (cmd->output[0]) {
            int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
            int fd_out = open(cmd->output, flags, 0644);
            if (fd_out < 0) { perror(cmd->output); _exit(127); }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        execvp(cmd->argv[0], cmd->argv);
        perror(cmd->argv[0]);
        _exit(127);
    } else if (pid > 0) {
        if (cmd->background) {
            printf("[Background PID: %d]\n", pid);
            last_exit_status = 0;
            return 0;
        }
        int status;
        waitpid(pid, &status, 0);
        int exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        last_exit_status = exit_status;
        return exit_status;
    } else {
        perror("fork");
        return -1;
    }
}

int execute_command(command_t *cmd) {
    if (cmd == NULL || cmd->argc == 0 || cmd->argv[0] == NULL) {
        return -1;
    }

    if (is_builtin(cmd->argv[0])) {
        int saved_stdout = -1, saved_stdin = -1;
        fflush(stdout);

        if (cmd->output[0]) {
            saved_stdout = dup(STDOUT_FILENO);
            int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
            int fd_out = open(cmd->output, flags, 0644);
            if (fd_out < 0) { perror(cmd->output); }
            else { dup2(fd_out, STDOUT_FILENO); close(fd_out); }
        }
        if (cmd->input[0]) {
            saved_stdin = dup(STDIN_FILENO);
            int fd_in = open(cmd->input, O_RDONLY);
            if (fd_in < 0) { perror(cmd->input); }
            else { dup2(fd_in, STDIN_FILENO); close(fd_in); }
        }

        builtin_status_t status = execute_builtin(cmd);

        if (saved_stdout != -1) { fflush(stdout); dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
        if (saved_stdin != -1) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }

        if (status == BUILTIN_EXIT) {
            shell_exit_requested = 1;
        }

        last_exit_status = 0;
        return 0;
    }

    return run_external(cmd);
}

int execute_pipeline(pipeline_t *pipeline) {
    if (pipeline == NULL || pipeline->command_count == 0) {
        return -1;
    }

    if (pipeline->command_count == 1) {
        return execute_command(&pipeline->commands[0]);
    }

    int n = pipeline->command_count;
    pid_t pids[MAX_COMMANDS];
    int previous_read = -1;

    int background = pipeline->commands[n - 1].background;

    for (int i = 0; i < n; i++) {
        command_t *cmd = &pipeline->commands[i];
        int pipefd[2] = { -1, -1 };

        if (i < n - 1) {
            if (pipe(pipefd) < 0) { perror("pipe"); return -1; }
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return -1; }

        if (pid == 0) {
            if (previous_read != -1) {
                dup2(previous_read, STDIN_FILENO);
            } else {
                if (background) {
                    int null_fd = open("/dev/null", O_RDONLY);
                    if (null_fd >= 0) {
                        dup2(null_fd, STDIN_FILENO);
                        close(null_fd);
                    }
                }
                if (cmd->input[0]) {
                    int fd_in = open(cmd->input, O_RDONLY);
                    if (fd_in < 0) { perror(cmd->input); _exit(127); }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }
            }

            if (i < n - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
            } else if (cmd->output[0]) {
                int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
                int fd_out = open(cmd->output, flags, 0644);
                if (fd_out < 0) { perror(cmd->output); _exit(127); }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            if (previous_read != -1) close(previous_read);
            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            if (is_builtin(cmd->argv[0])) {
                execute_builtin(cmd);
                fflush(stdout);
                _exit(0);
            }

            execvp(cmd->argv[0], cmd->argv);
            perror(cmd->argv[0]);
            _exit(127);
        }

        pids[i] = pid;
        if (previous_read != -1) close(previous_read);
        if (pipefd[1] != -1) close(pipefd[1]);
        previous_read = pipefd[0];
    }

    if (background) {
        printf("[Background Pipeline PID: %d]\n", pids[0]);
        last_exit_status = 0;
        return 0;
    }

    int last_status = -1;
    for (int i = 0; i < n; i++) {
        int wstatus;
        waitpid(pids[i], &wstatus, 0);
        int exit_status = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
        if (i == n - 1) last_status = exit_status;
    }

    last_exit_status = last_status;
    return last_status;
}

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
#include "../include/jobs.h"

extern int last_exit_status; /* defined in main.c */
int shell_exit_requested = 0;

/* ---------- MILESTONE-4.2 / 5.1: zombie prevention ---------- */
/* 4.2 used a global SIGCHLD handler that reaped EVERY child with
   waitpid(-1). That cannot coexist with job control: it would steal the
   exit/stop status of foreground children and of jobs in the job table,
   so `jobs`, `fg` and `bg` would see ECHILD and report wrong states.
   Zombies are still prevented, just by the job table instead:
     - foreground children are waited on synchronously (WUNTRACED),
     - every background job is in the job table and gets reaped by
       jobs_check_background() before each prompt.
   The function name is kept so existing callers (main.c) still link. */
void setup_background_handler(void) {
    /* Intentionally no SIGCHLD reaper - see the comment above. */
}

/* ---------- helpers ---------- */

static void build_command_string(command_t *cmd, char *out, size_t outsize) {
    out[0] = '\0';
    for (int i = 0; i < cmd->argc; i++) {
        strncat(out, cmd->argv[i], outsize - strlen(out) - 1);
        if (i != cmd->argc - 1) {
            strncat(out, " ", outsize - strlen(out) - 1);
        }
    }
}

static void build_pipeline_string(pipeline_t *pipeline, char *out, size_t outsize) {
    out[0] = '\0';
    for (int i = 0; i < pipeline->command_count; i++) {
        command_t *cmd = &pipeline->commands[i];
        for (int j = 0; j < cmd->argc; j++) {
            strncat(out, cmd->argv[j], outsize - strlen(out) - 1);
            if (j != cmd->argc - 1) {
                strncat(out, " ", outsize - strlen(out) - 1);
            }
        }
        if (i != pipeline->command_count - 1) {
            strncat(out, " | ", outsize - strlen(out) - 1);
        }
    }
}

/* tcsetpgrp() fails with ENOTTY when there is no controlling terminal
   (piped stdin, scripts). That is fine - skip the hand-off silently. */
static void give_terminal_to(pid_t pgid) {
    tcsetpgrp(STDIN_FILENO, pgid);
}

/* The shell ignores SIGINT/SIGQUIT/SIGTSTP/SIGTTIN/SIGTTOU for itself, but
   ignored dispositions are inherited across fork() AND survive execvp().
   Reset them in every child or jobs could never be stopped/interrupted. */
static void reset_child_signals(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

/* ---------- run an external (non-builtin) command ---------- */
static int run_external(command_t *cmd) {
    pid_t pid = fork();

    if (pid == 0) {
        setpgid(0, 0);            /* own process group (job control) */
        reset_child_signals();

        if (cmd->background) {    /* 4.2: background jobs never read the terminal */
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
        setpgid(pid, pid);

        if (cmd->background) {
            char command_string[MAX_JOB_COMMAND];
            build_command_string(cmd, command_string, sizeof(command_string));
            int job_id = job_add(pid, command_string, JOB_RUNNING);
            if (job_id > 0) {
                printf("[%d] %d\n", job_id, pid);
            }
            last_exit_status = 0;
            return 0;
        }

        /* Foreground: give the job the terminal, wait for it to finish
           OR stop (WUNTRACED), then take the terminal back. */
        give_terminal_to(pid);

        int status;
        waitpid(pid, &status, WUNTRACED);

        give_terminal_to(shell_pgid);

        if (WIFSTOPPED(status)) {
            char command_string[MAX_JOB_COMMAND];
            build_command_string(cmd, command_string, sizeof(command_string));
            int job_id = job_add(pid, command_string, JOB_STOPPED);
            if (job_id > 0) {
                printf("\n[%d]+  Stopped    %s\n", job_id, command_string);
            }
            last_exit_status = 0;
            return 0;
        }

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
    pid_t job_pgid = -1;

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
            /* All stages share one process group led by the first stage. */
            if (i == 0) {
                setpgid(0, 0);
            } else {
                setpgid(0, pids[0]);
            }
            reset_child_signals();

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
        if (i == 0) {
            setpgid(pid, pid);
            job_pgid = pid;
        } else {
            setpgid(pid, job_pgid);
        }

        if (previous_read != -1) close(previous_read);
        if (pipefd[1] != -1) close(pipefd[1]);
        previous_read = pipefd[0];
    }

    if (background) {
        char command_string[MAX_JOB_COMMAND];
        build_pipeline_string(pipeline, command_string, sizeof(command_string));
        int job_id = job_add(job_pgid, command_string, JOB_RUNNING);
        if (job_id > 0) {
            printf("[%d] %d\n", job_id, job_pgid);
        }
        last_exit_status = 0;
        return 0;
    }

    /* Foreground pipeline: hand over the terminal, wait for every stage
       (WUNTRACED to notice Ctrl+Z), then take the terminal back. */
    give_terminal_to(job_pgid);

    int last_status = -1;
    int stopped = 0;

    for (int i = 0; i < n; i++) {
        int wstatus;
        waitpid(pids[i], &wstatus, WUNTRACED);

        if (WIFSTOPPED(wstatus)) {
            stopped = 1;
        } else {
            int exit_status = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
            if (i == n - 1) last_status = exit_status;
        }
    }

    give_terminal_to(shell_pgid);

    if (stopped) {
        char command_string[MAX_JOB_COMMAND];
        build_pipeline_string(pipeline, command_string, sizeof(command_string));
        int job_id = job_add(job_pgid, command_string, JOB_STOPPED);
        if (job_id > 0) {
            printf("\n[%d]+  Stopped    %s\n", job_id, command_string);
        }
        last_exit_status = 0;
        return 0;
    }

    last_exit_status = last_status;
    return last_status;
}

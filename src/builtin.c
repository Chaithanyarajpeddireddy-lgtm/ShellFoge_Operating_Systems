#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "../include/builtin.h"
#include "../include/jobs.h"

extern int last_exit_status; /* defined in main.c */

/* ---------- cd ---------- */
static void builtin_cd(command_t *cmd) {
    char *dir;

    if (cmd->argc == 1) {
        dir = getenv("HOME");
        if (dir == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
    } else if (cmd->argc == 2) {
        dir = cmd->argv[1];
    } else {
        fprintf(stderr, "cd: too many arguments\n");
        return;
    }

    if (chdir(dir) != 0) {
        perror("cd");
    } else {
        printf("Changed directory to: %s\n", dir);
    }
}

/* ---------- pwd ---------- */
static void builtin_pwd(command_t *cmd) {
    if (cmd->argc > 1) {
        fprintf(stderr, "pwd: too many arguments\n");
        return;
    }

    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) == NULL) {
        perror("pwd");
        return;
    }

    printf("This is my directory: %s\n", buffer);
}

/* ---------- echo ---------- */
static void builtin_echo(command_t *cmd) {
    printf("Shellforge says: ");
    for (int i = 1; i < cmd->argc; i++) {
        printf("%s", cmd->argv[i]);
        if (i != cmd->argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
}

/* ---------- exit ---------- */
static void builtin_exit(command_t *cmd) {
    if (cmd->argc > 1) {
        fprintf(stderr, "exit: too many arguments\n");
    }
}

/* ---------- jobs ---------- */
static int builtin_jobs(command_t *cmd) {
    (void)cmd;
    jobs_print();
    return 0;
}

/* ---------- bg ---------- */
/* Resumes a stopped job in the background: SIGCONT to its whole process
   group, mark it Running, print "[n] cmd &". The job stays in the table
   because it is still running, just not in the foreground. */
static int builtin_bg(command_t *cmd) {
    if (cmd->argc < 2) {
        fprintf(stderr, "bg: usage: bg <job_id>\n");
        return 1;
    }

    int job_id = atoi(cmd->argv[1]);
    job_t *job = job_find(job_id);
    if (job == NULL) {
        fprintf(stderr, "bg: no such job %d\n", job_id);
        return 1;
    }

    kill(-job->pgid, SIGCONT);
    job_continue(job->pgid);
    printf("[%d] %s &\n", job->job_id, job->command);
    return 0;
}

/* ---------- fg ---------- */
/* Brings a job to the foreground: SIGCONT if it was stopped, hand it the
   terminal so Ctrl+C/Ctrl+Z reach it instead of the shell, wait for it,
   then take the terminal back. If it is stopped again (Ctrl+Z) it keeps
   the same job number; it is only removed once it actually finishes. */
static int builtin_fg(command_t *cmd) {
    if (cmd->argc < 2) {
        fprintf(stderr, "fg: usage: fg <job_id>\n");
        return 1;
    }

    int job_id = atoi(cmd->argv[1]);
    job_t *job = job_find(job_id);
    if (job == NULL) {
        fprintf(stderr, "fg: no such job %d\n", job_id);
        return 1;
    }

    pid_t pgid = job->pgid;
    char command_copy[MAX_JOB_COMMAND];
    strncpy(command_copy, job->command, sizeof(command_copy) - 1);
    command_copy[sizeof(command_copy) - 1] = '\0';

    if (job->state == JOB_STOPPED) {
        kill(-pgid, SIGCONT);
    }
    job_continue(pgid);

    printf("%s\n", command_copy);

    tcsetpgrp(STDIN_FILENO, pgid);          /* job gets the terminal */

    int status;
    waitpid(-pgid, &status, WUNTRACED);

    tcsetpgrp(STDIN_FILENO, shell_pgid);    /* shell takes it back */

    if (WIFSTOPPED(status)) {
        job_stop(pgid);
        printf("\n[%d]+  Stopped    %s\n", job_id, command_copy);
    } else {
        job_remove(job_id);
        last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    return 0;
}

/* ---------- dispatch ---------- */
int is_builtin(const char *name) {
    if (name == NULL) return 0;
    return (strcmp(name, "cd")   == 0 ||
            strcmp(name, "pwd")  == 0 ||
            strcmp(name, "echo") == 0 ||
            strcmp(name, "exit") == 0 ||
            strcmp(name, "jobs") == 0 ||
            strcmp(name, "fg")   == 0 ||
            strcmp(name, "bg")   == 0);
}

builtin_status_t execute_builtin(command_t *cmd) {
    if (cmd->argc == 0 || cmd->argv[0] == NULL) {
        return BUILTIN_NOT_FOUND;
    }

    if (strcmp(cmd->argv[0], "cd") == 0) {
        builtin_cd(cmd);
        return BUILTIN_HANDLED;
    }
    if (strcmp(cmd->argv[0], "pwd") == 0) {
        builtin_pwd(cmd);
        return BUILTIN_HANDLED;
    }
    if (strcmp(cmd->argv[0], "echo") == 0) {
        builtin_echo(cmd);
        return BUILTIN_HANDLED;
    }
    if (strcmp(cmd->argv[0], "exit") == 0) {
        builtin_exit(cmd);
        return BUILTIN_EXIT;
    }
    if (strcmp(cmd->argv[0], "jobs") == 0) {
        builtin_jobs(cmd);
        return BUILTIN_HANDLED;
    }
    if (strcmp(cmd->argv[0], "fg") == 0) {
        builtin_fg(cmd);
        return BUILTIN_HANDLED;
    }
    if (strcmp(cmd->argv[0], "bg") == 0) {
        builtin_bg(cmd);
        return BUILTIN_HANDLED;
    }

    return BUILTIN_NOT_FOUND;
}

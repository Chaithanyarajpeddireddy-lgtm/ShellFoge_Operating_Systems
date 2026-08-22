#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/builtin.h"

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

    printf("%s\n", buffer);
}

/* ---------- echo ---------- */
static void builtin_echo(command_t *cmd) {
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

int is_builtin(const char *name) {
    if (name == NULL) return 0;
    return (strcmp(name, "cd")   == 0 ||
            strcmp(name, "pwd")  == 0 ||
            strcmp(name, "echo") == 0 ||
            strcmp(name, "exit") == 0);
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

    return BUILTIN_NOT_FOUND;
}

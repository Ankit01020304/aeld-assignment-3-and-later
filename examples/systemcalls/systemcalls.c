#include "systemcalls.h"

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 * successfully using the system() call, false otherwise
 */
bool do_system(const char *cmd)
{
    int status = system(cmd);

    if (status == -1)
    {
        return false;
    }

    return WIFEXITED(status) &&
           (WEXITSTATUS(status) == 0);
}

/**
 * @param count The number of arguments passed
 * @param ... command and arguments
 * @return true on success
 */
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);

    char *command[count + 1];
    int i;

    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }

    command[count] = NULL;

    va_end(args);

    fflush(stdout);

    pid_t pid = fork();

    if (pid < 0)
    {
        return false;
    }

    if (pid == 0)
    {
        execv(command[0], command);

        exit(EXIT_FAILURE);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
    {
        return false;
    }

    return WIFEXITED(status) &&
           (WEXITSTATUS(status) == 0);
}

/**
 * @param outputfile output file path
 * @param count number of command arguments
 * @param ... command arguments
 * @return true on success
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);

    char *command[count + 1];
    int i;

    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }

    command[count] = NULL;

    va_end(args);

    fflush(stdout);

    pid_t pid = fork();

    if (pid < 0)
    {
        return false;
    }

    if (pid == 0)
    {
        int fd = open(outputfile,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0644);

        if (fd < 0)
        {
            exit(EXIT_FAILURE);
        }

        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            close(fd);
            exit(EXIT_FAILURE);
        }

        close(fd);

        execv(command[0], command);

        exit(EXIT_FAILURE);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
    {
        return false;
    }

    return WIFEXITED(status) &&
           (WEXITSTATUS(status) == 0);
}

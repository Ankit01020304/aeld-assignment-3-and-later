#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static volatile sig_atomic_t exit_requested = 0;

void signal_handler(int signo)
{
    (void)signo;
    exit_requested = 1;
}

int main(int argc, char *argv[])
{
    int server_fd = -1;
    int client_fd = -1;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    struct sigaction sa;
    bool daemon_mode = false;

    if ((argc == 2) && (strcmp(argv[1], "-d") == 0))
    {
        daemon_mode = true;
    }

    openlog("aesdsocket", 0, LOG_USER);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return -1;
    }

    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 10) == -1)
    {
        perror("listen");
        close(server_fd);
        return -1;
    }

    if (daemon_mode)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            close(server_fd);
            return -1;
        }

        if (pid > 0)
        {
            exit(EXIT_SUCCESS);
        }

        if (setsid() < 0)
        {
            close(server_fd);
            return -1;
        }

        chdir("/");

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    while (!exit_requested)
    {
        client_len = sizeof(client_addr);

        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);

        if (client_fd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        char client_ip[INET_ADDRSTRLEN];

        inet_ntop(AF_INET,
                  &client_addr.sin_addr,
                  client_ip,
                  sizeof(client_ip));

        syslog(LOG_INFO,
               "Accepted connection from %s",
               client_ip);

        char *packet = NULL;
        size_t packet_size = 0;
        bool packet_complete = false;

        while (!packet_complete)
        {
            char recv_buffer[BUFFER_SIZE];

            ssize_t bytes_received =
                recv(client_fd,
                     recv_buffer,
                     sizeof(recv_buffer),
                     0);

            if (bytes_received <= 0)
            {
                break;
            }

            char *new_packet =
                realloc(packet,
                        packet_size + bytes_received);

            if (new_packet == NULL)
            {
                free(packet);
                packet = NULL;
                break;
            }

            packet = new_packet;

            memcpy(packet + packet_size,
                   recv_buffer,
                   bytes_received);

            packet_size += bytes_received;

            if (memchr(recv_buffer,
                       '\n',
                       bytes_received) != NULL)
            {
                packet_complete = true;
            }
        }

        if (packet && packet_complete)
        {
            FILE *fp = fopen(DATA_FILE, "a");

            if (fp)
            {
                fwrite(packet,
                       1,
                       packet_size,
                       fp);

                fclose(fp);
            }

            fp = fopen(DATA_FILE, "r");

            if (fp)
            {
                char send_buffer[BUFFER_SIZE];
                size_t bytes_read;

                while ((bytes_read =
                        fread(send_buffer,
                              1,
                              sizeof(send_buffer),
                              fp)) > 0)
                {
                    send(client_fd,
                         send_buffer,
                         bytes_read,
                         0);
                }

                fclose(fp);
            }
        }

        free(packet);

        close(client_fd);

        syslog(LOG_INFO,
               "Closed connection from %s",
               client_ip);
    }

    syslog(LOG_INFO, "Caught signal, exiting");

    if (client_fd >= 0)
    {
        close(client_fd);
    }

    if (server_fd >= 0)
    {
        close(server_fd);
    }

    unlink(DATA_FILE);

    closelog();

    return 0;
}

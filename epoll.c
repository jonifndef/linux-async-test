// epoll_server_stress.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <time.h>

#define PORT 8080
#define BACKLOG 65535
#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096
#define RESPONSE_SIZE 65536   // 64 KB
#define ARTIFICIAL_DELAY_US 1000  // 1ms simulated work

typedef struct {
    int fd;
    size_t written;
} client_t;

static char response[RESPONSE_SIZE];

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // Fill response buffer
    memset(response, 'A', sizeof(response));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    set_nonblocking(server_fd);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, BACKLOG);

    int epfd = epoll_create1(0);

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = server_fd
    };
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    struct epoll_event events[MAX_EVENTS];

    printf("epoll stress server running...\n");

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == server_fd) {

                while (1) {
                    int client_fd = accept(server_fd, NULL, NULL);
                    if (client_fd < 0) {
                        if (errno == EAGAIN) break;
                        perror("accept");
                        break;
                    }

                    set_nonblocking(client_fd);

                    client_t *client = malloc(sizeof(client_t));
                    client->fd = client_fd;
                    client->written = 0;

                    struct epoll_event cev = {
                        .events = EPOLLIN | EPOLLET,
                        .data.ptr = client
                    };

                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
                }

            } else {
                client_t *client = events[i].data.ptr;
                int fd = client->fd;

                if (events[i].events & EPOLLIN) {
                    char buf[BUFFER_SIZE];
                    ssize_t r = read(fd, buf, sizeof(buf));
                    if (r <= 0) {
                        close(fd);
                        free(client);
                        continue;
                    }

                    // Simulated work
                    usleep(ARTIFICIAL_DELAY_US);

                    struct epoll_event mod = {
                        .events = EPOLLOUT | EPOLLET,
                        .data.ptr = client
                    };
                    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &mod);
                }

                if (events[i].events & EPOLLOUT) {
                    while (client->written < RESPONSE_SIZE) {
                        ssize_t w = write(fd,
                                          response + client->written,
                                          RESPONSE_SIZE - client->written);

                        if (w < 0) {
                            if (errno == EAGAIN) break;
                            close(fd);
                            free(client);
                            goto next_event;
                        }

                        client->written += w;
                    }

                    if (client->written >= RESPONSE_SIZE) {
                        client->written = 0;

                        struct epoll_event mod = {
                            .events = EPOLLIN | EPOLLET,
                            .data.ptr = client
                        };
                        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &mod);
                    }
                }
            }

            next_event:;
        }
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_EVENTS 16

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family=AF_INET, .sin_addr.s_addr=htonl(INADDR_ANY), .sin_port=htons(8080) };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("bind");
      exit(1);
    }

    listen(server_fd, 128);

    int epfd = epoll_create1(0);

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = server_fd
    };

    struct epoll_event events[MAX_EVENTS];

    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    const char resp[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello world!";

    for (;;) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == server_fd) {
                int c = accept(server_fd, NULL, NULL);
                if (c >= 0) {
                  ev.events = EPOLLIN;
                  ev.data.fd = c;
                  epoll_ctl(epfd, EPOLL_CTL_ADD, c, &ev);
                }
            } else {
                int c = events[i].data.fd;
                char buf[1024];
                ssize_t r = recv(c, buf, sizeof(buf), 0);
                if (r > 0) write(c, resp, sizeof(resp) - 1);
                epoll_ctl(epfd, EPOLL_CTL_DEL, c, NULL);
                close(c);
            }
        }
    }
}

// minimal_uring_server.c: tiny io_uring-based HTTP server using accept(), recv(), write()
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <liburing.h>

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family=AF_INET, .sin_addr.s_addr=htonl(INADDR_ANY), .sin_port=htons(8080) };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("bind");
      exit(1);
    }

    listen(server_fd, 128);

    struct io_uring ring; io_uring_queue_init(64, &ring, 0);

    const char resp[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello world!";

    for (;;) {
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)&cli, &clilen, 0);
        io_uring_submit(&ring);
        struct io_uring_cqe *cqe; io_uring_wait_cqe(&ring, &cqe);
        int c = cqe->res; io_uring_cqe_seen(&ring, cqe);
        if (c < 0) continue;

        char buf[1024];
        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe, c, buf, sizeof(buf), 0);
        io_uring_submit(&ring);
        io_uring_wait_cqe(&ring, &cqe);
        ssize_t r = cqe->res; io_uring_cqe_seen(&ring, cqe);

        if (r > 0) {
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_write(sqe, c, resp, sizeof(resp) - 1, 0);
            io_uring_submit(&ring);
            io_uring_wait_cqe(&ring, &cqe);
            io_uring_cqe_seen(&ring, cqe);
        }

        close(c);
    }
}

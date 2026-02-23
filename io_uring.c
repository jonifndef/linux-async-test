#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <time.h>

#define PORT 8080
#define BACKLOG 65535
#define QUEUE_DEPTH 4096
#define BUFFER_SIZE 4096
#define BODY_SIZE 65536
#define ARTIFICIAL_DELAY_US 1000

enum {
    OP_ACCEPT,
    OP_READ,
    OP_WRITE
};

typedef struct {
    int fd;
    int type;
    size_t written;
} conn_info;

static char *response;
static size_t response_size;

void build_response() {
    const char *header_fmt =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    int header_len = snprintf(NULL, 0, header_fmt, BODY_SIZE);
    char *header = malloc(header_len + 1);
    sprintf(header, header_fmt, BODY_SIZE);

    response_size = header_len + BODY_SIZE;
    response = malloc(response_size);

    memcpy(response, header, header_len);
    memset(response + header_len, 'A', BODY_SIZE);

    free(header);
}

int main() {
    build_response();

    struct io_uring ring;
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    params.flags = IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2000;

    if (io_uring_queue_init_params(QUEUE_DEPTH, &ring, &params) < 0) {
        perror("io_uring_queue_init");
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("bind");
      exit(1);
    }

    listen(server_fd, 128);
    listen(server_fd, BACKLOG);

    printf("io_uring stress server running...\n");

    // Submit first accept
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    conn_info *accept_info = malloc(sizeof(conn_info));
    accept_info->fd = server_fd;
    accept_info->type = OP_ACCEPT;
    io_uring_prep_accept(sqe, server_fd, NULL, NULL, 0);
    io_uring_sqe_set_data(sqe, accept_info);
    io_uring_submit(&ring);

    while (1) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
            continue;

        conn_info *info = io_uring_cqe_get_data(cqe);

        if (info->type == OP_ACCEPT) {
            int client_fd = cqe->res;

            // Resubmit accept
            struct io_uring_sqe *sqe2 = io_uring_get_sqe(&ring);
            io_uring_prep_accept(sqe2, server_fd, NULL, NULL, 0);
            io_uring_sqe_set_data(sqe2, info);
            io_uring_submit(&ring);

            if (client_fd >= 0) {
                conn_info *read_info = malloc(sizeof(conn_info));
                read_info->fd = client_fd;
                read_info->type = OP_READ;
                read_info->written = 0;

                char *buf = malloc(BUFFER_SIZE);

                struct io_uring_sqe *sqe3 = io_uring_get_sqe(&ring);
                io_uring_prep_read(sqe3, client_fd, buf, BUFFER_SIZE, 0);
                io_uring_sqe_set_data(sqe3, read_info);
                io_uring_submit(&ring);
            }

        } else if (info->type == OP_READ) {
            if (cqe->res <= 0) {
                close(info->fd);
                free(info);
            } else {
                usleep(ARTIFICIAL_DELAY_US);

                info->type = OP_WRITE;
                info->written = 0;

                struct io_uring_sqe *sqe4 = io_uring_get_sqe(&ring);
                io_uring_prep_write(sqe4, info->fd, response, response_size, 0);
                io_uring_sqe_set_data(sqe4, info);
                io_uring_submit(&ring);
            }

        } else if (info->type == OP_WRITE) {
            if (cqe->res <= 0) {
                close(info->fd);
                free(info);
            } else {
                info->written += cqe->res;

                if (info->written < response_size) {
                    struct io_uring_sqe *sqe5 = io_uring_get_sqe(&ring);
                    io_uring_prep_write(
                        sqe5,
                        info->fd,
                        response + info->written,
                        response_size - info->written,
                        info->written);
                    io_uring_sqe_set_data(sqe5, info);
                    io_uring_submit(&ring);
                } else {
                    // keep-alive: go back to read
                    info->type = OP_READ;
                    char *buf = malloc(BUFFER_SIZE);

                    struct io_uring_sqe *sqe6 = io_uring_get_sqe(&ring);
                    io_uring_prep_read(sqe6, info->fd, buf, BUFFER_SIZE, 0);
                    io_uring_sqe_set_data(sqe6, info);
                    io_uring_submit(&ring);
                }
            }
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    return 0;
}

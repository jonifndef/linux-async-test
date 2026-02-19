// blocking_server_stress.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define BACKLOG 65535
#define BUFFER_SIZE 4096
#define ARTIFICIAL_DELAY_US 1000  // 1 ms

#define BODY_SIZE 65536

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

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Blocking stress server running...\n");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        while (1) {
            char buffer[BUFFER_SIZE];

            ssize_t r = read(client_fd, buffer, sizeof(buffer));
            if (r <= 0) {
                break; // client closed or error
            }

            // Simulated work
            usleep(ARTIFICIAL_DELAY_US);

            // Write full 64KB response (handle partial writes)
            size_t written = 0;
            while (written < response_size) {
                ssize_t w = write(client_fd,
                                  response + written,
                                  response_size - written);

                if (w <= 0) {
                    if (errno == EINTR)
                        continue;
                    goto close_connection;
                }

                written += w;
            }

            // Keep-alive: loop and wait for next request
        }

close_connection:
        close(client_fd);
    }

    return 0;
}

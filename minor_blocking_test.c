#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      perror("socket");
      exit(1);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("bind");
      exit(1);
    }
    if (listen(server_fd, 1) < 0) {
      perror("listen");
      exit(1);
    }

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len); // blocking accept() (or connect() if we are a client)
    if (client_fd < 0) {
      perror("accept");
      exit(1);
    }

    char buf[1024];
    ssize_t n = recv(client_fd, buf, sizeof(buf), 0); // blocking recv (or read() when doing file I/O)

    const char *resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello world!";

    write(client_fd, resp, strlen(resp)); // blocking write()

    close(client_fd);
    close(server_fd);
    return 0;
}

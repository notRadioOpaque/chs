#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    int server_fd;
    int rc;
    int client_fd;
    socklen_t client_len;
    char in_buf[1024];
    ssize_t in_n_bytes;
    char out_buf[1024];
    ssize_t out_n_bytes;

    char*  body = "Hello, world!\n";
    size_t body_len = strlen(body);

    char headers[1024];
    snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %lu\r\n"
        "\r\n",
        body_len);

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    /* create socket -> listen file descriptor */
    server_fd = socket(PF_INET, SOCK_STREAM, 0);

    if (server_fd < 0 ) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof server_addr); // zero-ing the server addr struct to avoid surprises :)
    server_addr.sin_port = htons(4000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;

    /* binding the socket to a port */
    rc = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (rc < 0) {
        perror("Failed to bind to port");
        exit(EXIT_FAILURE);
    }

    /* listening at the bounded port */
    rc = listen(server_fd, 5);

    if (rc < 0) {
        perror("Failed to listen on port:4000");
        exit(EXIT_FAILURE);
    }

    memset(&client_addr, 0, sizeof(client_addr)); // zero-ing the client addr struct to avoid surprises :)
    client_len = sizeof(client_addr);

    /* accepting client connection (blocking) */
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
        perror("Failed to accept incoming requests");
        exit(EXIT_FAILURE);
    }

    /* read incoming bytes from client */
    in_n_bytes = read(client_fd, in_buf, sizeof(in_buf) - 1);

    if (in_n_bytes < 0) {
        perror("failed to read bytes from request");
        exit(EXIT_FAILURE);
    }

    in_buf[in_n_bytes] = '\0'; // null terminate so we don't read beyond what we should

    printf("%s", in_buf);

    /* compose response for the client */
    out_buf[0] = '\0';
    strlcat(out_buf, headers, sizeof(out_buf));
    strlcat(out_buf, body, sizeof(out_buf));

    /* write response back to client */
    out_n_bytes = write(client_fd, out_buf, strlen(out_buf));

    if(out_n_bytes < 0) {
        perror("failed to write response back to client");
        exit(EXIT_FAILURE);
    }

    printf("%s\n", out_buf);

    return 0;
}

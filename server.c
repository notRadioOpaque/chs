#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_pid_t.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/wait.h>

struct http_request {
    char method[8];
    char path[256];
    char version[16];
};

int parse_request(char* raw_request, struct http_request *out) {
    char *request_line = strstr(raw_request, "\r\n");
    if (request_line == NULL) return -1;
    *request_line = '\0';

    for (int i = 0; i < 3; i++) {
        char *token = strtok((i == 0 ? raw_request : NULL), " ");
        if (token == NULL) return -1;

        if (i == 0) {
            strlcpy(out->method, token, sizeof(out->method));
        } else if (i == 1) {
            strlcpy(out->path, token, sizeof(out->path));
        } else {
            strlcpy(out->version, token, sizeof(out->version));
        }
    }

    return 0;
}

void send_response(int client_fd, int status_code, const char *reason, const char *body) {
    char out_buf[1024];
    ssize_t out_n_bytes;
    char headers[1024];

    size_t body_len = strlen(body);

    snprintf(headers, sizeof(headers),
        "HTTP/1.1 %i %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %lu\r\n"
        "\r\n",
        status_code, reason, body_len);

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
}

int handle_client(int client_fd) {
    char in_buf[1024];
    ssize_t in_n_bytes;
    char*  body = "Hello, world!\n";

    struct http_request req;

    /* read incoming bytes from client */
    in_n_bytes = read(client_fd, in_buf, sizeof(in_buf) - 1);

    if (in_n_bytes < 0) {
        perror("failed to read bytes from request");
        exit(EXIT_FAILURE);
    }

    in_buf[in_n_bytes] = '\0'; // null terminate so we don't read beyond what we should

    int res = parse_request(in_buf, &req);

    if (res < 0) {
        send_response(client_fd, 400, "Bad Request", "");
        close(client_fd);

        return -1;
    }

    send_response(client_fd, 200, "Ok", body);
    close(client_fd);

    return 0;
}

int main() {
    int server_fd;
    int rc;

    struct sockaddr_in server_addr;

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

    while (1) {
        int client_fd;
        socklen_t client_len;

        struct sockaddr_in client_addr;

        memset(&client_addr, 0, sizeof(client_addr)); // zero-ing the client addr struct to avoid surprises :)
        client_len = sizeof(client_addr);

        /* accepting client connection (blocking) */
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            perror("Failed to accept incoming requests");
            exit(EXIT_FAILURE);
        }

        /* fork happens here to create 2 process, parent and child */
        /* parent falls back to accepting new connections and child handles the rest pertaining to the accepted conn... */
        pid_t result = fork();

        if (result < 0) {
            perror("fork failed");
            close(client_fd);
        } else if (result == 0) {
            // we are inside the child process
            handle_client(client_fd);
            exit(EXIT_SUCCESS);
        } else {
            // we are inside the parent process
            close(client_fd);
            waitpid(-1, NULL, WNOHANG);
            continue;
        }
    }

    return 0;
}

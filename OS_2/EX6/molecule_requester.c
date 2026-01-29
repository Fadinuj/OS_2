#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#define BUFFER_SIZE 1024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    char *server_ip = NULL;
    int port = -1;
    char *uds_socket_path = NULL;
    int use_uds = 0;

    int opt;
    while ((opt = getopt(argc, argv, "h:p:f:")) != -1) {
        switch (opt) {
            case 'h':
                server_ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                uds_socket_path = optarg;
                use_uds = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-h <server_ip> -p <port>] OR [-f <UDS_socket_path>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (use_uds) {
        if (!uds_socket_path) {
            fprintf(stderr, "Error: -f <UDS_socket_path> is required for UDS mode.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        if (!server_ip || port <= 0) {
            fprintf(stderr, "Error: both -h <server_ip> and -p <port> are required for UDP mode.\n");
            exit(EXIT_FAILURE);
        }
    }

    int sockfd;
    char input[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    if (use_uds) {
        sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("UDS socket failed");
            exit(EXIT_FAILURE);
        }

        // קביעת כתובת זמנית ללקוח
        struct sockaddr_un client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        snprintf(client_addr.sun_path, sizeof(client_addr.sun_path), "/tmp/client_%d", getpid());
        unlink(client_addr.sun_path); // מניעת התנגשויות

        if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        // הגדרת כתובת השרת
        struct sockaddr_un server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, uds_socket_path, sizeof(server_addr.sun_path) - 1);

        printf("Connected to UDS server at %s\n", uds_socket_path);
        printf("Enter DELIVER commands (e.g., DELIVER WATER 2), or type EXIT to quit:\n");

        while (1) {
            printf("> ");
            if (!fgets(input, sizeof(input), stdin)) break;

            input[strcspn(input, "\r\n")] = 0;

            if (strcmp(input, "EXIT") == 0) break;

            if (sendto(sockfd, input, strlen(input), 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("sendto failed");
                continue;
            }

            socklen_t addrlen = sizeof(server_addr);
            int bytes_received = recvfrom(sockfd, response, sizeof(response) - 1, 0,
                                          (struct sockaddr *)&server_addr, &addrlen);
            if (bytes_received < 0) {
                perror("recvfrom failed");
                continue;
            }

            response[bytes_received] = '\0';
            printf("Server response: %s\n", response);
        }

        unlink(client_addr.sun_path); // ניקוי בסיום

    } else {
        // מצב UDP רגיל (לא UDS)
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            perror("invalid address");
            exit(EXIT_FAILURE);
        }

        printf("Connected to UDP server at %s:%d\n", server_ip, port);
        printf("Enter DELIVER commands (e.g., DELIVER WATER 2), or type EXIT to quit:\n");

        while (1) {
            printf("> ");
            if (!fgets(input, sizeof(input), stdin)) break;

            input[strcspn(input, "\r\n")] = 0;

            if (strcmp(input, "EXIT") == 0) break;

            if (sendto(sockfd, input, strlen(input), 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("sendto failed");
                continue;
            }

            socklen_t addrlen = sizeof(server_addr);
            int bytes_received = recvfrom(sockfd, response, sizeof(response) - 1, 0,
                                          (struct sockaddr *)&server_addr, &addrlen);
            if (bytes_received < 0) {
                perror("recvfrom failed");
                continue;
            }

            response[bytes_received] = '\0';
            printf("Server response: %s\n", response);
        }
    }

    close(sockfd);
    return 0;
}

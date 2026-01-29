#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    char *server_ip = NULL;
    int port = -1;

    int opt;
    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
            case 'h':
                server_ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -h <server_ip> -p <port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!server_ip || port <= 0) {
        fprintf(stderr, "Error: both -h <server_ip> and -p <port> are required.\n");
        exit(EXIT_FAILURE);
    }


    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    char input[BUFFER_SIZE];
    char response[BUFFER_SIZE];

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

    close(sockfd);
    return 0;
}

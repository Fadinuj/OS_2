#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    char *host = NULL;
    char *port_str = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "h:p:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            host = optarg;
            break;
        case 'p':
            port_str = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s -h <host> -p <port>\n", argv[0]);
            return 1;
        }
    }

    if (!host || !port_str)
    {
        fprintf(stderr, "Error: -h <host> and -p <port> are required.\n");
        return 1;
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0)
    {
        perror("socket failed");
        freeaddrinfo(res);
        return 1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("Connection failed");
        freeaddrinfo(res);
        close(sock);
        return 1;
    }

    printf("Connected to server at %s:%s\n", host, port_str);
    freeaddrinfo(res);

    char buffer[BUFFER_SIZE];

    while (1)
    {
        printf("Enter command ADD CARBON , OXYGEN , HYDROGEN: ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            printf("Error reading input\n");
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strcmp(buffer, "EXIT") == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            printf("send EXIT to server - client requests to disconnect\n");
            break;
        }

        if (strcmp(buffer, "SHUTDOWN") == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            printf("Sent SHUTDOWN. Closing client.\n");
            break;
        }

        ssize_t bytes_sent = send(sock, buffer, strlen(buffer), 0);
        if (bytes_sent < 0)
        {
            perror("Send failed");
            break;
        }

        char recv_buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_received = recv(sock, recv_buffer, sizeof(recv_buffer), 0);
        if (bytes_received <= 0)
        {
            printf("No response from server or connection error.\n");
            break;
        }

        if (bytes_received == sizeof(unsigned int))
        {
            unsigned int count = 0;
            memcpy(&count, recv_buffer, sizeof(unsigned int));

            char dummy_action[16], atom_type[32];
            long long dummy_amount;
            if (sscanf(buffer, "%15s %31s %lld", dummy_action, atom_type, &dummy_amount) == 3)
                printf("Server returned count: %s = %u\n", atom_type, count);
            else
                printf("Server returned count = %u\n", count);
        }
        else
        {
            recv_buffer[bytes_received] = '\0';
            printf("Server response: %s\n", recv_buffer);
        }
    }

    close(sock);
    return 0;
}

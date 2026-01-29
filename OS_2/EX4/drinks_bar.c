#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_CLIENTS FD_SETSIZE
#define BUFFER_SIZE 1024

volatile sig_atomic_t shutdown_requested = 0;

typedef struct timeval timeval;

void handle_shutdown(int sig)
{
    shutdown_requested = 1;
}

unsigned long long min(unsigned long long a, unsigned long long b)
{
    return (a < b) ? a : b;
}

unsigned long long min3(unsigned long long a, unsigned long long b, unsigned long long c)
{
    return min(min(a, b), c);
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_shutdown);

    unsigned long long carbon = 0;
    unsigned long long oxygen = 0;
    unsigned long long hydrogen = 0;
    int timeout = -1;
    int tcp_port = -1;
    int udp_port = -1;

    static struct option long_options[] = {
        {"oxygen", required_argument, 0, 'o'},
        {"carbon", required_argument, 0, 'c'},
        {"hydrogen", required_argument, 0, 'h'},
        {"timeout", required_argument, 0, 't'},
        {"tcp-port", required_argument, 0, 'T'},
        {"udp-port", required_argument, 0, 'U'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "o:c:h:t:T:U:", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'o':
            oxygen = strtoull(optarg, NULL, 10);
            break;
        case 'c':
            carbon = strtoull(optarg, NULL, 10);
            break;
        case 'h':
            hydrogen = strtoull(optarg, NULL, 10);
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        case 'T':
            tcp_port = atoi(optarg);
            break;
        case 'U':
            udp_port = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Usage: %s -T <tcp_port> -U <udp_port> [--oxygen N | -o N] [--carbon N | -c N] [--hydrogen N | -h N] [--timeout N | -t N]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (tcp_port < 0 || udp_port < 0)
    {
        fprintf(stderr, "Error: --tcp-port (-T) and --udp-port (-U) are required.\n");
        exit(EXIT_FAILURE);
    }

    printf("Starting with H=%llu, O=%llu, C=%llu, timeout=%d\n", hydrogen, oxygen, carbon, timeout);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("TCP socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(tcp_port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("TCP bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("TCP listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0)
    {
        perror("UDP socket failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);

    if (bind(udp_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
    {
        perror("UDP bind failed");
        close(server_fd);
        close(udp_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on TCP %d and UDP %d\n", tcp_port, udp_port);

    int clients[MAX_CLIENTS] = {0};
    time_t last_activity = time(NULL);

    while (!shutdown_requested)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(udp_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int max_fd = server_fd > udp_fd ? server_fd : udp_fd;
        if (STDIN_FILENO > max_fd)
            max_fd = STDIN_FILENO;

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i] > 0)
            {
                FD_SET(clients[i], &readfds);
                if (clients[i] > max_fd)
                    max_fd = clients[i];
            }
        }

        timeval tv;
        timeval *tv_ptr = NULL;
        if (timeout > 0)
        {
            int elapsed = time(NULL) - last_activity;
            if (elapsed >= timeout)
            {
                printf("Timeout of %d seconds reached. Server shutting down.\n", timeout);
                break;
            }
            tv.tv_sec = timeout - elapsed;
            tv.tv_usec = 0;
            tv_ptr = &tv;
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, tv_ptr);
        if (activity < 0 && !shutdown_requested)
        {
            perror("select error");
            break;
        }

        if (activity > 0)
        {
            last_activity = time(NULL);
        }

        if (FD_ISSET(server_fd, &readfds))
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket < 0)
            {
                perror("accept failed");
                continue;
            }

            printf("New client connected: %s\n", inet_ntoa(client_addr.sin_addr));

            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients[i] == 0)
                {
                    clients[i] = new_socket;
                    break;
                }
            }
        }

        if (FD_ISSET(udp_fd, &readfds))
        {
            char buffer[BUFFER_SIZE] = {0};
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int bytes_received = recvfrom(udp_fd, buffer, sizeof(buffer) - 1, 0,
                                          (struct sockaddr *)&client_addr, &client_len);
            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';
                buffer[strcspn(buffer, "\r\n")] = 0;

                char action[32], part1[32], part2[32];
                unsigned long long amount = 0;
                char item[64] = "";

                int count = sscanf(buffer, "%31s %31s %31s %llu", action, part1, part2, &amount);

                if (count == 3 && strcmp(action, "DELIVER") == 0)
                {
                    strcpy(item, part1);
                    amount = strtoull(part2, NULL, 10);
                }
                else if (count == 4 && strcmp(action, "DELIVER") == 0)
                {
                    snprintf(item, sizeof(item), "%s_%s", part1, part2);
                }
                else
                {
                    const char *response = "CANNOT_DELIVER";
                    sendto(udp_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                    continue;
                }

                if (amount == 0)
                {
                    const char *response = "CANNOT_DELIVER";
                    sendto(udp_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                    continue;
                }

                unsigned long long need_H = 0, need_O = 0, need_C = 0;

                if (strcmp(item, "WATER") == 0)
                {
                    need_H = 2 * amount;
                    need_O = 1 * amount;
                }
                else if (strcmp(item, "CARBON_DIOXIDE") == 0)
                {
                    need_C = 1 * amount;
                    need_O = 2 * amount;
                }
                else if (strcmp(item, "GLUCOSE") == 0)
                {
                    need_C = 6 * amount;
                    need_H = 12 * amount;
                    need_O = 6 * amount;
                }
                else if (strcmp(item, "ALCOHOL") == 0)
                {
                    need_C = 2 * amount;
                    need_H = 6 * amount;
                    need_O = 1 * amount;
                }
                else
                {
                    const char *response = "CANNOT_DELIVER";
                    sendto(udp_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                    continue;
                }

                if (hydrogen >= need_H && oxygen >= need_O && carbon >= need_C)
                {
                    hydrogen -= need_H;
                    oxygen -= need_O;
                    carbon -= need_C;

                    printf("UDP DELIVER: %s (%llu units)\n", item, amount);
                    printf("Atoms warehouse after delivery: H=%llu, O=%llu, C=%llu\n", hydrogen, oxygen, carbon);

                    const char *response = "DELIVERED";
                    sendto(udp_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                }
                else
                {
                    const char *response = "CANNOT_DELIVER";
                    sendto(udp_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                }
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            char input[BUFFER_SIZE];
            if (fgets(input, sizeof(input), stdin))
            {
                input[strcspn(input, "\r\n")] = 0;
                char command[32], drink[32];
                if (sscanf(input, "%31s %31s", command, drink) == 2 && strcmp(command, "GEN") == 0)
                {
                    int can_make = 0;
                    if (strcmp(drink, "SOFT") == 0 || strcmp(drink, "SOFTDRINK") == 0 || strcmp(drink, "SOFT_DRINK") == 0)
                    {
                        can_make = min3(hydrogen / 14, oxygen / 9, carbon / 7);
                        printf("Can make %d SOFT DRINK(s)\n", can_make);
                    }
                    else if (strcmp(drink, "VODKA") == 0)
                    {
                        can_make = min3(hydrogen / 20, oxygen / 8, carbon / 8);
                        printf("Can make %d VODKA(s)\n", can_make);
                    }
                    else if (strcmp(drink, "CHAMPAGNE") == 0)
                    {
                        can_make = min3(hydrogen / 8, oxygen / 4, carbon / 3);
                        printf("Can make %d CHAMPAGNE(s)\n", can_make);
                    }
                    else
                    {
                        printf(" Unknown drink type.\n");
                    }
                }
                else
                {
                    printf(" Invalid GEN command.\n");
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            int client_fd = clients[i];
            if (client_fd > 0 && FD_ISSET(client_fd, &readfds))
            {
                char buffer[BUFFER_SIZE] = {0};
                int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes_received <= 0)
                {
                    printf(" Client disconnected.\n");
                    close(client_fd);
                    clients[i] = 0;
                    continue;
                }

                buffer[strcspn(buffer, "\r\n")] = 0;

                if (strcmp(buffer, "EXIT") == 0)
                {
                    printf(" Client requested EXIT.\n");
                    close(client_fd);
                    clients[i] = 0;
                    continue;
                }

                if (strcmp(buffer, "SHUTDOWN") == 0)
                {
                    printf(" Shutdown command received from client.\n");
                    shutdown_requested = 1;
                    close(client_fd);
                    clients[i] = 0;
                    break;
                }

                char action[16], atom_type[32];
                long long amount = -1;

                if (sscanf(buffer, "%15s %31s %lld", action, atom_type, &amount) != 3)
                {
                    const char *error_msg = "ERROR: Invalid command format. Use: ADD <ATOM_TYPE> <POSITIVE_AMOUNT>\n";
                    send(client_fd, error_msg, strlen(error_msg), 0);
                    continue;
                }

                if (strcmp(action, "ADD") != 0)
                {
                    const char *error_msg = "ERROR: Unknown command. Only 'ADD' is supported.\n";
                    send(client_fd, error_msg, strlen(error_msg), 0);
                    continue;
                }

                if (amount <= 0)
                {
                    const char *error_msg = "ERROR: Amount must be a positive number greater than zero.\n";
                    send(client_fd, error_msg, strlen(error_msg), 0);
                    continue;
                }

                unsigned long long *target = NULL;
                if (strcmp(atom_type, "HYDROGEN") == 0)
                {
                    target = &hydrogen;
                }
                else if (strcmp(atom_type, "OXYGEN") == 0)
                {
                    target = &oxygen;
                }
                else if (strcmp(atom_type, "CARBON") == 0)
                {
                    target = &carbon;
                }
                else
                {
                    const char *error_msg = "ERROR: Unknown atom type. Must be HYDROGEN, OXYGEN or CARBON.\n";
                    send(client_fd, error_msg, strlen(error_msg), 0);
                    continue;
                }

                *target += amount;
                printf(" Atoms warehouse: H=%llu, O=%llu, C=%llu\n", hydrogen, oxygen, carbon);

                unsigned int response = (unsigned int)(*target);
                send(client_fd, &response, sizeof(response), 0);
              }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i] > 0)
                close(clients[i]);
        }
        close(server_fd);
        close(udp_fd);

        printf("Server shut down.....\n");
        return 0;
    }

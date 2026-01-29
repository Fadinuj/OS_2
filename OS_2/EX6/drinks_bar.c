#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_CLIENTS FD_SETSIZE
#define BUFFER_SIZE 1024

volatile sig_atomic_t shutdown_requested = 0;
char *save_file_path = NULL;

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

void cleanup_uds_files(const char *stream_path, const char *datagram_path, const char *socket_path)
{
    if (stream_path)
        unlink(stream_path);
    if (datagram_path)
        unlink(datagram_path);
    if (socket_path)
        unlink(socket_path);
}

// Save server state to file
int save_state(unsigned long long hydrogen, unsigned long long oxygen, unsigned long long carbon, const char *filename)
{
    if (!filename)
        return 0;

    FILE *file = fopen(filename, "w");
    if (!file)
    {
        perror("Failed to open save file for writing");
        return -1;
    }

    fprintf(file, "%llu %llu %llu\n", hydrogen, oxygen, carbon);
    fclose(file);
    printf("State saved to %s: H=%llu, O=%llu, C=%llu\n", filename, hydrogen, oxygen, carbon);
    return 0;
}

// Load server state from file
int load_state(unsigned long long *hydrogen, unsigned long long *oxygen, unsigned long long *carbon, const char *filename)
{
    if (!filename)
        return 0;

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Save file %s not found, starting with default values\n", filename);
        return 0; // Not an error, just no saved state
    }

    if (fscanf(file, "%llu %llu %llu", hydrogen, oxygen, carbon) != 3)
    {
        fprintf(stderr, "Error reading save file %s\n", filename);
        fclose(file);
        return -1;
    }

    fclose(file);
    printf("State loaded from %s: H=%llu, O=%llu, C=%llu\n", filename, *hydrogen, *oxygen, *carbon);
    return 1; // Successfully loaded
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
    char *uds_stream_path = NULL;
    char *uds_datagram_path = NULL;
    char *uds_socket_path = NULL;

    static struct option long_options[] = {
        {"oxygen", required_argument, 0, 'o'},
        {"carbon", required_argument, 0, 'c'},
        {"hydrogen", required_argument, 0, 'h'},
        {"timeout", required_argument, 0, 't'},
        {"tcp-port", required_argument, 0, 'T'},
        {"udp-port", required_argument, 0, 'U'},
        {"stream-path", required_argument, 0, 's'},
        {"datagram-path", required_argument, 0, 'd'},
        {"socket-path", required_argument, 0, 'f'}, // For backward compatibility
        {"save-file", required_argument, 0, 'F'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "o:c:h:t:T:U:s:d:f:F:", long_options, NULL)) != -1)
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
        case 's':
            uds_stream_path = optarg;
            break;
        case 'd':
            uds_datagram_path = optarg;
            break;
        case 'f':
            uds_socket_path = optarg;
            break;
        case 'F':
            save_file_path = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s [--tcp-port|-T <port>] [--udp-port|-U <port>] [--stream-path|-s <path>] [--datagram-path|-d <path>] [--socket-path|-f <path>] [--save-file|-F <file>] [options...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Check that at least one socket type is configured
    if (tcp_port < 0 && udp_port < 0 && !uds_stream_path && !uds_datagram_path && !uds_socket_path)
    {
        fprintf(stderr, "Error: At least one socket type must be configured (TCP, UDP, or UDS).\n");
        exit(EXIT_FAILURE);
    }

    // Try to load state from file if save file is specified
    int state_loaded = 0;
    if (save_file_path)
    {
        int load_result = load_state(&hydrogen, &oxygen, &carbon, save_file_path);
        if (load_result > 0)
        {
            state_loaded = 1;
        }
        else if (load_result < 0)
        {
            exit(EXIT_FAILURE);
        }
    }

    printf("Starting with H=%llu, O=%llu, C=%llu, timeout=%d\n", hydrogen, oxygen, carbon, timeout);
    if (save_file_path)
    {
        printf("Save file: %s\n", save_file_path);
    }

    // Socket file descriptors
    int server_fd = -1, udp_fd = -1, uds_stream_fd = -1, uds_datagram_fd = -1;

    // Setup TCP socket if requested
    if (tcp_port >= 0)
    {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
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
        printf("TCP server listening on port %d\n", tcp_port);
    }

    // Setup UDP socket if requested
    if (udp_port >= 0)
    {
        udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_fd < 0)
        {
            perror("UDP socket failed");
            if (server_fd >= 0)
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
            if (server_fd >= 0)
                close(server_fd);
            close(udp_fd);
            exit(EXIT_FAILURE);
        }
        printf("UDP server listening on port %d\n", udp_port);
    }

    // Setup UDS stream socket if requested
    if (uds_stream_path || uds_socket_path)
    {
        const char *stream_path = uds_stream_path ? uds_stream_path : uds_socket_path;

        uds_stream_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (uds_stream_fd < 0)
        {
            perror("UDS stream socket failed");
            cleanup_uds_files(NULL, NULL, NULL);
            if (server_fd >= 0)
                close(server_fd);
            if (udp_fd >= 0)
                close(udp_fd);
            exit(EXIT_FAILURE);
        }

        struct sockaddr_un stream_addr;
        memset(&stream_addr, 0, sizeof(stream_addr));
        stream_addr.sun_family = AF_UNIX;
        strncpy(stream_addr.sun_path, stream_path, sizeof(stream_addr.sun_path) - 1);

        unlink(stream_path); // Remove existing socket file
        if (bind(uds_stream_fd, (struct sockaddr *)&stream_addr, sizeof(stream_addr)) < 0)
        {
            perror("UDS stream bind failed");
            cleanup_uds_files(stream_path, NULL, NULL);
            if (server_fd >= 0)
                close(server_fd);
            if (udp_fd >= 0)
                close(udp_fd);
            close(uds_stream_fd);
            exit(EXIT_FAILURE);
        }

        if (listen(uds_stream_fd, 5) < 0)
        {
            perror("UDS stream listen failed");
            cleanup_uds_files(stream_path, NULL, NULL);
            if (server_fd >= 0)
                close(server_fd);
            if (udp_fd >= 0)
                close(udp_fd);
            close(uds_stream_fd);
            exit(EXIT_FAILURE);
        }
        printf("UDS stream socket listening on %s\n", stream_path);
    }

    // Setup UDS datagram socket if requested
    if (uds_datagram_path)
    {
        uds_datagram_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (uds_datagram_fd < 0)
        {
            perror("UDS datagram socket failed");
            cleanup_uds_files(uds_stream_path, NULL, uds_socket_path);
            if (server_fd >= 0)
                close(server_fd);
            if (udp_fd >= 0)
                close(udp_fd);
            if (uds_stream_fd >= 0)
                close(uds_stream_fd);
            exit(EXIT_FAILURE);
        }

        struct sockaddr_un datagram_addr;
        memset(&datagram_addr, 0, sizeof(datagram_addr));
        datagram_addr.sun_family = AF_UNIX;
        strncpy(datagram_addr.sun_path, uds_datagram_path, sizeof(datagram_addr.sun_path) - 1);

        unlink(uds_datagram_path); // Remove existing socket file
        if (bind(uds_datagram_fd, (struct sockaddr *)&datagram_addr, sizeof(datagram_addr)) < 0)
        {
            perror("UDS datagram bind failed");
            cleanup_uds_files(uds_stream_path, uds_datagram_path, uds_socket_path);
            if (server_fd >= 0)
                close(server_fd);
            if (udp_fd >= 0)
                close(udp_fd);
            if (uds_stream_fd >= 0)
                close(uds_stream_fd);
            close(uds_datagram_fd);
            exit(EXIT_FAILURE);
        }
        printf("UDS datagram socket listening on %s\n", uds_datagram_path);
    }

    int clients[MAX_CLIENTS] = {0};
    time_t last_activity = time(NULL);

    while (!shutdown_requested)
    {
        fd_set readfds;
        FD_ZERO(&readfds);

        // Add all active sockets to the fd_set
        if (server_fd >= 0)
            FD_SET(server_fd, &readfds);
        if (udp_fd >= 0)
            FD_SET(udp_fd, &readfds);
        if (uds_stream_fd >= 0)
            FD_SET(uds_stream_fd, &readfds);
        if (uds_datagram_fd >= 0)
            FD_SET(uds_datagram_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int max_fd = STDIN_FILENO;
        if (server_fd >= 0 && server_fd > max_fd)
            max_fd = server_fd;
        if (udp_fd >= 0 && udp_fd > max_fd)
            max_fd = udp_fd;
        if (uds_stream_fd >= 0 && uds_stream_fd > max_fd)
            max_fd = uds_stream_fd;
        if (uds_datagram_fd >= 0 && uds_datagram_fd > max_fd)
            max_fd = uds_datagram_fd;

        // Add client sockets
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

        // Handle TCP connections
        if (server_fd >= 0 && FD_ISSET(server_fd, &readfds))
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket < 0)
            {
                perror("TCP accept failed");
            }
            else
            {
                printf("New TCP client connected: %s\n", inet_ntoa(client_addr.sin_addr));
                for (int i = 0; i < MAX_CLIENTS; ++i)
                {
                    if (clients[i] == 0)
                    {
                        clients[i] = new_socket;
                        break;
                    }
                }
            }
        }

        // Handle UDS stream connections
        if (uds_stream_fd >= 0 && FD_ISSET(uds_stream_fd, &readfds))
        {
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(uds_stream_fd, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket < 0)
            {
                perror("UDS stream accept failed");
            }
            else
            {
                printf("New UDS stream client connected\n");
                for (int i = 0; i < MAX_CLIENTS; ++i)
                {
                    if (clients[i] == 0)
                    {
                        clients[i] = new_socket;
                        break;
                    }
                }
            }
        }

        // Handle UDP datagrams
        if (udp_fd >= 0 && FD_ISSET(udp_fd, &readfds))
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

                // Process DELIVER command (same logic as original)
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

                    // Save state after each delivery if save file is configured
                    if (save_file_path)
                    {
                        save_state(hydrogen, oxygen, carbon, save_file_path);
                    }

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

        // Handle UDS datagrams
        if (uds_datagram_fd >= 0 && FD_ISSET(uds_datagram_fd, &readfds))
        {
            char buffer[BUFFER_SIZE] = {0};
            struct sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            int bytes_received = recvfrom(uds_datagram_fd, buffer, sizeof(buffer) - 1, 0,
                                          (struct sockaddr *)&client_addr, &client_len);
            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';
                buffer[strcspn(buffer, "\r\n")] = 0;

                // Same DELIVER logic as UDP
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
                    sendto(uds_datagram_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                    continue;
                }

                if (amount == 0)
                {
                    const char *response = "CANNOT_DELIVER";
                    sendto(uds_datagram_fd, response, strlen(response), 0,
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
                    sendto(uds_datagram_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                    continue;
                }

                if (hydrogen >= need_H && oxygen >= need_O && carbon >= need_C)
                {
                    hydrogen -= need_H;
                    oxygen -= need_O;
                    carbon -= need_C;

                    printf("UDS DELIVER: %s (%llu units)\n", item, amount);
                    printf("Atoms warehouse after delivery: H=%llu, O=%llu, C=%llu\n", hydrogen, oxygen, carbon);

                    // Save state after each delivery if save file is configured
                    if (save_file_path)
                    {
                        save_state(hydrogen, oxygen, carbon, save_file_path);
                    }

                    const char *response = "DELIVERED";
                    sendto(uds_datagram_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                }
                else
                {
                    const char *response = "CANNOT_DELIVER";
                    sendto(uds_datagram_fd, response, strlen(response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                }
            }
        }

        // Handle stdin input (GEN commands)
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

        // Handle client messages (TCP and UDS stream)
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

                // Save state after each ADD if save file is configured
                if (save_file_path)
                {
                    save_state(hydrogen, oxygen, carbon, save_file_path);
                }

                unsigned int response = (unsigned int)(*target);
                send(client_fd, &response, sizeof(response), 0);
            }

        }
    }
            // Save final state before shutdown
            if (save_file_path)
            {
                save_state(hydrogen, oxygen, carbon, save_file_path);
            }

            // Cleanup
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients[i] > 0)
                    close(clients[i]);
            }

            if (server_fd >= 0)
                close(server_fd);
            if (udp_fd >= 0)
                close(udp_fd);
            if (uds_stream_fd >= 0)
                close(uds_stream_fd);
            if (uds_datagram_fd >= 0)
                close(uds_datagram_fd);

            cleanup_uds_files(uds_stream_path, uds_datagram_path, uds_socket_path);

            printf("Server shut down.....\n");
            return 0;
        }
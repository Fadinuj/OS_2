#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>

#define MAX_CLIENTS FD_SETSIZE //  Default size at which select() can track maximum number of open files.
#define BUFFER_SIZE 1024

volatile sig_atomic_t shutdown_requested = 0; // When a SIGINT signal arrives (for example, pressing Ctrl+C), the server will know to terminate execution.

void handle_shutdown(int sig)
{
    shutdown_requested = 1;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_shutdown);

    // Variables that represent the amount of atoms in the warehouse.
    unsigned long long carbon = 0;
    unsigned long long oxygen = 0;
    unsigned long long hydrogen = 0;

    // Check that when running the server, exactly one argument was sent.
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    // create TCP

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", port);

    int clients[MAX_CLIENTS] = {0};

    while (!shutdown_requested)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i] > 0)
            {
                FD_SET(clients[i], &readfds);
                if (clients[i] > max_fd)
                    max_fd = clients[i];
            }
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && !shutdown_requested)
        {
            perror("select error");
            break;
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

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            int client_fd = clients[i];
            if (client_fd > 0 && FD_ISSET(client_fd, &readfds))
            {
                char buffer[BUFFER_SIZE] = {0};
                int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes_received <= 0)
                {
                    printf("Client disconnected.\n");
                    close(client_fd);
                    clients[i] = 0;
                    continue;
                }

                if (strcmp(buffer, "EXIT") == 0)
                {
                    printf("Client requested EXIT.\n");
                    close(client_fd);
                    clients[i] = 0;
                    continue;
                }

                buffer[strcspn(buffer, "\r\n")] = 0;

                if (strcmp(buffer, "SHUTDOWN") == 0)
                {
                    printf("Shutdown\n");
                    shutdown_requested = 1;
                    close(client_fd);
                    clients[i] = 0;
                    break;
                }

                char action[16], atom_type[32];
                long long amount = -1;

                if (sscanf(buffer, "%15s %31s %lld", action, atom_type, &amount) != 3)
                {
                    unsigned int status = 0; 
                    send(client_fd, &status, sizeof(status), 0);
                    const char *error_msg = "Error: Invalid format. Use: ADD <ATOM_TYPE> <POSITIVE_AMOUNT>";
                    send(client_fd, error_msg, strlen(error_msg) + 1, 0); 
                    continue;
                }

                if (strcmp(action, "ADD") != 0 || amount <= 0)
                {
                    unsigned int status = 0; 
                    send(client_fd, &status, sizeof(status), 0);
                    const char *error_msg = "Error: Invalid operation or negative/zero amount. Use: ADD <ATOM_TYPE> <POSITIVE_AMOUNT>";
                    send(client_fd, error_msg, strlen(error_msg) + 1, 0);
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
                    unsigned int status = 0; 
                    send(client_fd, &status, sizeof(status), 0);
                    const char *error_msg = "Error: Invalid atom type. Use only: HYDROGEN, OXYGEN, or CARBON";
                    send(client_fd, error_msg, strlen(error_msg) + 1, 0);
                    continue;
                }

            
                *target += (unsigned long long)amount;
                printf("Atoms warehouse: H=%llu, O=%llu, C=%llu\n", hydrogen, oxygen, carbon);

                unsigned int status = 1; 
                send(client_fd, &status, sizeof(status), 0);
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
        printf("Server shut down.....\n");

        return 0;
    }

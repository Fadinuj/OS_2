#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  TCP: %s -h <host> -p <port>\n", program_name);
    fprintf(stderr, "  UDS: %s -f <socket_path>\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --host <host>         Connect to TCP server at host\n");
    fprintf(stderr, "  -p, --port <port>         Connect to TCP server at port\n");
    fprintf(stderr, "  -f, --socket <path>       Connect to UDS server at socket path\n");
    fprintf(stderr, "  --help                    Show this help message\n");
}

int connect_tcp(const char *host, const char *port_str)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0)
    {
        perror("TCP socket failed");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("TCP connection failed");
        freeaddrinfo(res);
        close(sock);
        return -1;
    }

    printf("Connected to TCP server at %s:%s\n", host, port_str);
    freeaddrinfo(res);
    return sock;
}

int connect_uds(const char *socket_path)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("UDS socket failed");
        return -1;
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;

    if (strlen(socket_path) >= sizeof(server_addr.sun_path))
    {
        fprintf(stderr, "Socket path too long: %s\n", socket_path);
        close(sock);
        return -1;
    }

    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("UDS connection failed");
        close(sock);
        return -1;
    }

    printf("Connected to UDS server at %s\n", socket_path);
    return sock;
}

int main(int argc, char *argv[])
{
    char *host = NULL;
    char *port_str = NULL;
    char *socket_path = NULL;

    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"socket", required_argument, 0, 'f'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:f:", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            host = optarg;
            break;
        case 'p':
            port_str = optarg;
            break;
        case 'f':
            socket_path = optarg;
            break;
        case '?':
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate arguments
    int tcp_mode = (host != NULL && port_str != NULL);
    int uds_mode = (socket_path != NULL);

    if (!tcp_mode && !uds_mode)
    {
        fprintf(stderr, "Error: Must specify either TCP (-h and -p) or UDS (-f) connection.\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (tcp_mode && uds_mode)
    {
        fprintf(stderr, "Error: Cannot specify both TCP and UDS connections simultaneously.\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (tcp_mode && (!host || !port_str))
    {
        fprintf(stderr, "Error: For TCP connection, both -h <host> and -p <port> are required.\n\n");
        print_usage(argv[0]);
        return 1;
    }

    // Establish connection
    int sock = -1;
    if (tcp_mode)
    {
        sock = connect_tcp(host, port_str);
    }
    else if (uds_mode)
    {
        sock = connect_uds(socket_path);
    }

    if (sock < 0)
    {
        fprintf(stderr, "Failed to establish connection.\n");
        return 1;
    }

    char buffer[BUFFER_SIZE];

    printf("Connection established. You can now send commands.\n");
    printf("Available commands:\n");
    printf("  ADD HYDROGEN <amount>   - Add hydrogen atoms\n");
    printf("  ADD OXYGEN <amount>     - Add oxygen atoms\n");
    printf("  ADD CARBON <amount>     - Add carbon atoms\n");
    printf("  EXIT                    - Disconnect from server\n");
    printf("  SHUTDOWN                - Shutdown server\n\n");

    while (1)
    {
        printf("Enter command: ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            printf("Error reading input or EOF reached\n");
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;

        // Handle empty input
        if (strlen(buffer) == 0)
        {
            continue;
        }

        if (strcmp(buffer, "EXIT") == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            printf("Sent EXIT to server - client requests to disconnect\n");
            break;
        }

        if (strcmp(buffer, "SHUTDOWN") == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            printf("Sent %s. Closing client.\n", buffer);
            break;
        }

        // Parse and validate ADD command
        char atom_type[32] = "";
        long long amount;
        char action[16];

        if (sscanf(buffer, "%15s %31s %lld", action, atom_type, &amount) == 3 &&
            strcmp(action, "ADD") == 0 && amount > 0)
        {
            // Validate atom type
            if (strcmp(atom_type, "HYDROGEN") != 0 &&
                strcmp(atom_type, "OXYGEN") != 0 &&
                strcmp(atom_type, "CARBON") != 0)
            {
                printf("Error: Invalid atom type '%s'. Valid types: HYDROGEN, OXYGEN, CARBON\n", atom_type);
                continue;
            }

            ssize_t bytes_sent = send(sock, buffer, strlen(buffer), 0);
            if (bytes_sent < 0)
            {
                perror("Send failed");
                break;
            }

            // Expect a binary response (unsigned int) for valid ADD commands
            char response_buffer[BUFFER_SIZE] = {0};
            ssize_t bytes_received = recv(sock, response_buffer, sizeof(response_buffer) - 1, 0);

            if (bytes_received <= 0)
            {
                perror("Failed to receive response from server");
                break;
            }

            if (bytes_received == sizeof(unsigned int))
            {
                unsigned int response = 0;
                memcpy(&response, response_buffer, sizeof(unsigned int));
                printf("Server returned count: %s = %u\n", atom_type, response);
            }
            else
            {
                response_buffer[bytes_received] = '\0';
                printf("Server error: %s", response_buffer);
            }
        }

            else
            {
                // Send invalid command and expect text error response
                ssize_t bytes_sent = send(sock, buffer, strlen(buffer), 0);
                if (bytes_sent < 0)
                {
                    perror("Send failed");
                    break;
                }

                char error_response[BUFFER_SIZE] = {0};
                ssize_t bytes_received = recv(sock, error_response, sizeof(error_response) - 1, 0);
                if (bytes_received > 0)
                {
                    error_response[bytes_received] = '\0';
                    printf("Server response: %s", error_response);
                }
                else if (bytes_received == 0)
                {
                    printf("Server closed connection.\n");
                    break;
                }
                else
                {
                    perror("Failed to receive response from server");
                    break;
                }
            }
        }

        close(sock);
        printf("Connection closed.\n");
        return 0;
    }
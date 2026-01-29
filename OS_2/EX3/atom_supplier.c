#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    const char *port_str = argv[2];

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    int status = getaddrinfo(server_ip, port_str, &hints, &res);
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

    printf("Connected to server at %s:%s\n", server_ip, port_str);
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
            printf("send EXIT to server- client requests to disconnect\n");
            break;
        }

        if (strcmp(buffer, "SHUTDOWN") == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            printf("Sent %s. Closing client.\n", buffer);
            break;
        }

        char atom_type[32] = "";
        long long dummy_amount;
        char dummy_action[16];

        // שליחת הפקודה לשרת
        ssize_t bytes_sent = send(sock, buffer, strlen(buffer), 0);
        if (bytes_sent < 0)
        {
            perror("Send failed");
            break;
        }

        // בדיקה אם הפקודה תקינה
        if (sscanf(buffer, "%15s %31s %lld", dummy_action, atom_type, &dummy_amount) == 3 &&
            strcmp(dummy_action, "ADD") == 0 && dummy_amount > 0)
        {
            // פקודה תקינה - מצפים לקבל unsigned int
            char response_buffer[BUFFER_SIZE] = {0};
            ssize_t bytes_received = recv(sock, response_buffer, sizeof(response_buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                fprintf(stderr, "Failed to receive response from server\n");
                break;
            }
            response_buffer[bytes_received] = '\0';

            // בדיקה אם התשובה היא טקסט שמתחיל ב־"ERROR"
            if (strncmp(response_buffer, "ERROR", 5) == 0)
            {
                printf("Server error: %s", response_buffer);
            }
            else
            {
                // אחרת נניח שזה מספר בינארי
                unsigned int response = 0;
                memcpy(&response, response_buffer, sizeof(unsigned int));
                printf("Server returned count: %s = %u\n", atom_type, response);
            }
          }
          

            else
            {
                char error_buffer[BUFFER_SIZE] = {0};
                ssize_t bytes_received = recv(sock, error_buffer, sizeof(error_buffer) - 1, 0);
                if (bytes_received <= 0)
                {
                    fprintf(stderr, "Failed to receive error message from server\n");
                    break;
                }
                error_buffer[bytes_received] = '\0';
                printf("Server error: %s", error_buffer);
            }
        }
        close(sock);
        return 0;
    }
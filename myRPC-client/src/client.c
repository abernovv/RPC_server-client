#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "libmysyslog.h"  // Подключение библиотеки логирования

#define BUFFER_SIZE 1024

void print_help() {
    printf("Usage: myRPC-client [OPTIONS]\n");
    printf("Options:\n");
    printf("  -c, --command \"bash_command\"   Command to execute\n");
    printf("  -h, --host \"ip_addr\"          Server IP address\n");
    printf("  -p, --port PORT                Server port\n");
    printf("  -s, --stream                   Use stream socket\n");
    printf("  -d, --dgram                    Use datagram socket\n");
    printf("      --help                     Display this help and exit\n");
}

int main(int argc, char *argv[]) {
    char *command = NULL;
    char *server_ip = NULL;
    int port = 0;
    int use_stream = 1; // Default to stream socket
    int opt;

    static struct option long_options[] = {
        {"command", required_argument, 0, 'c'},
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"stream", no_argument, 0, 's'},
        {"dgram", no_argument, 0, 'd'},
        {"help", no_argument, 0,  0 },
        {0, 0, 0, 0}
    };

    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "c:h:p:sd", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                command = optarg;
                break;
            case 'h':
                server_ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                use_stream = 1;
                break;
            case 'd':
                use_stream = 0;
                break;
            case 0:
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }

    if (!command || !server_ip || !port) {
        fprintf(stderr, "Error: Missing required arguments\n");
        print_help();
        return 1;
    }

    // Получение имени пользователя
    struct passwd *pw = getpwuid(getuid());
    char *username = pw->pw_name;

    // Подготовка запроса
    char request[BUFFER_SIZE];
    snprintf(request, BUFFER_SIZE, "%s: %s", username, command);

    // Логирование действия
    mysyslog("Connecting to the server...", INFO, 0, 0, "/var/log/myrpc.log");

    // Создание сокета
    int sockfd;
    if (use_stream) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (sockfd < 0) {
        mysyslog("Socket creation failed", ERROR, 0, 0, "/var/log/myrpc.log");
        perror("Socket creation failed");
        return 1;
    }

    // Настройка адреса сервера
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    if (use_stream) {
        // Подключение к серверу
        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            mysyslog("Connection failed", ERROR, 0, 0, "/var/log/myrpc.log");
            perror("Connection failed");
            close(sockfd);
            return 1;
        }

        // Логирование успешного подключения
        mysyslog("Successfully connected to server", INFO, 0, 0, "/var/log/myrpc.log");

        // Отправка запроса
        send(sockfd, request, strlen(request), 0);

        // Получение ответа
        char response[BUFFER_SIZE];
        int n = recv(sockfd, response, BUFFER_SIZE, 0);
        response[n] = '\0';
        printf("Server response: %s\n", response);

        // Логирование результата
        mysyslog("Received server response", INFO, 0, 0, "/var/log/myrpc.log");

    } else {
        // Отправка запроса
        sendto(sockfd, request, strlen(request), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));

        // Получение ответа
        char response[BUFFER_SIZE];
        socklen_t len = sizeof(servaddr);
        int n = recvfrom(sockfd, response, BUFFER_SIZE, 0, (struct sockaddr*)&servaddr, &len);
        response[n] = '\0';
        printf("Server response: %s\n", response);

        // Логирование результата
        mysyslog("Received server response", INFO, 0, 0, "/var/log/myrpc.log");
    }

    close(sockfd);
    return 0;
}


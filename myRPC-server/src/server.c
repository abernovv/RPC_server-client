#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "config_parser.h"
#include "libmysyslog.h"  // Подключение библиотеки логирования

#define BUFFER_SIZE 1024

volatile sig_atomic_t stop;

void handle_signal(int sig) {
    stop = 1;
}

int user_allowed(const char *username) {
    FILE *file = fopen("/etc/myRPC/users.conf", "r");
    if (!file) {
        mysyslog("Failed to open users.conf", ERROR, 0, 0, "/var/log/myrpc.log");
        perror("Failed to open users.conf");
        return 0;
    }

    char line[256];
    int allowed = 0;
    while (fgets(line, sizeof(line), file)) {
        // Удаление символа новой строки
        line[strcspn(line, "\n")] = '\0';

        // Пропуск комментариев и пустых строк
        if (line[0] == '#' || strlen(line) == 0)
            continue;

        if (strcmp(line, username) == 0) {
            allowed = 1;
            break;
        }
    }

    fclose(file);
    return allowed;
}

void execute_command(const char *command, char *stdout_file, char *stderr_file) {
    char cmd[BUFFER_SIZE];
    snprintf(cmd, BUFFER_SIZE, "%s >%s 2>%s", command, stdout_file, stderr_file);
    system(cmd);
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Чтение конфигурационного файла
    Config config = parse_config("/etc/myRPC/myRPC.conf");

    int port = config.port;
    int use_stream = strcmp(config.socket_type, "stream") == 0;

    // Логирование старта сервера
    mysyslog("Server starting...", INFO, 0, 0, "/var/log/myrpc.log");

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

    // Установка параметра SO_REUSEADDR для повторного использования порта
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        mysyslog("setsockopt failed", ERROR, 0, 0, "/var/log/myrpc.log");
        perror("setsockopt failed");
        close(sockfd);
        return 1;
    }

    // Привязка сокета к адресу
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        mysyslog("Bind failed", ERROR, 0, 0, "/var/log/myrpc.log");
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    if (use_stream) {
        listen(sockfd, 5);
        mysyslog("Server listening (stream)", INFO, 0, 0, "/var/log/myrpc.log");
    } else {
        mysyslog("Server listening (datagram)", INFO, 0, 0, "/var/log/myrpc.log");
    }

    while (!stop) {
        char buffer[BUFFER_SIZE];
        int n;

        if (use_stream) {
            len = sizeof(cliaddr);
            int connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);
            if (connfd < 0) {
                mysyslog("Accept failed", ERROR, 0, 0, "/var/log/myrpc.log");
                perror("Accept failed");
                continue;
            }

            n = recv(connfd, buffer, BUFFER_SIZE, 0);
            if (n <= 0) {
                close(connfd);
                continue;
            }
            buffer[n] = '\0';

            // Логирование полученного запроса
            mysyslog("Received request", INFO, 0, 0, "/var/log/myrpc.log");

            // Обработка запроса
            char *username = strtok(buffer, ":");
            char *command = strtok(NULL, "");
            if (command) {
                while (*command == ' ')
                    command++;
            }

            char response[BUFFER_SIZE];

            if (user_allowed(username)) {
                mysyslog("User allowed", INFO, 0, 0, "/var/log/myrpc.log");

                // Выполнение команды
                char stdout_file[] = "/tmp/myRPC_XXXXXX.stdout";
                char stderr_file[] = "/tmp/myRPC_XXXXXX.stderr";
                mkstemp(stdout_file);
                mkstemp(stderr_file);

                execute_command(command, stdout_file, stderr_file);

                // Чтение вывода команды
                FILE *f = fopen(stdout_file, "r");
                if (f) {
                    size_t read_bytes = fread(response, 1, BUFFER_SIZE, f);
                    response[read_bytes] = '\0';
                    fclose(f);
                    mysyslog("Command executed successfully", INFO, 0, 0, "/var/log/myrpc.log");
                } else {
                    strcpy(response, "Error reading stdout file");
                    mysyslog("Error reading stdout file", ERROR, 0, 0, "/var/log/myrpc.log");
                }

                remove(stdout_file);
                remove(stderr_file);

            } else {
                snprintf(response, BUFFER_SIZE, "1: User '%s' is not allowed", username);
                mysyslog("User not allowed", WARN, 0, 0, "/var/log/myrpc.log");
            }

            send(connfd, response, strlen(response), 0);
            close(connfd);

        } else {
            len = sizeof(cliaddr);
            n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&cliaddr, &len);
            if (n <= 0) {
                continue;
            }
            buffer[n] = '\0';

            // Логирование полученного запроса
            mysyslog("Received request", INFO, 0, 0, "/var/log/myrpc.log");

            // Обработка запроса
            char *username = strtok(buffer, ":");
            char *command = strtok(NULL, "");
            if (command) {
                while (*command == ' ')
                    command++;
            }

            char response[BUFFER_SIZE];

            if (user_allowed(username)) {
                mysyslog("User allowed", INFO, 0, 0, "/var/log/myrpc.log");

                // Выполнение команды
                char stdout_file[] = "/tmp/myRPC_XXXXXX.stdout";
                char stderr_file[] = "/tmp/myRPC_XXXXXX.stderr";
                mkstemp(stdout_file);
                mkstemp(stderr_file);

                execute_command(command, stdout_file, stderr_file);

                // Чтение вывода команды
                FILE *f = fopen(stdout_file, "r");
                if (f) {
                    size_t read_bytes = fread(response, 1, BUFFER_SIZE, f);
                    response[read_bytes] = '\0';
                    fclose(f);
                    mysyslog("Command executed successfully", INFO, 0, 0, "/var/log/myrpc.log");
                } else {
                    strcpy(response, "Error reading stdout file");
                    mysyslog("Error reading stdout file", ERROR, 0, 0, "/var/log/myrpc.log");
                }

                remove(stdout_file);
                remove(stderr_file);

            } else {
                snprintf(response, BUFFER_SIZE, "1: User '%s' is not allowed", username);
                mysyslog("User not allowed", WARN, 0, 0, "/var/log/myrpc.log");
            }

            sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&cliaddr, len);
        }
    }

    close(sockfd);
    mysyslog("Server stopped", INFO, 0, 0, "/var/log/myrpc.log");
    return 0;
}

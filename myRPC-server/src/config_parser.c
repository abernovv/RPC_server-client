#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config_parser.h"

Config parse_config(const char *filename) {
    Config config;
    config.port = 0;
    strcpy(config.socket_type, "stream");

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open config file");
        return config;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Удаление символа новой строки
        line[strcspn(line, "\n")] = '\0';

        // Пропуск комментариев и пустых строк
        if (line[0] == '#' || strlen(line) == 0)
            continue;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "");

        if (strcmp(key, "port") == 0) {
            config.port = atoi(value);
        } else if (strcmp(key, "socket_type") == 0) {
            strcpy(config.socket_type, value);
        }
    }

    fclose(file);
    return config;
}

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

typedef struct {
    int port;
    char socket_type[16];
} Config;

Config parse_config(const char *filename);

#endif

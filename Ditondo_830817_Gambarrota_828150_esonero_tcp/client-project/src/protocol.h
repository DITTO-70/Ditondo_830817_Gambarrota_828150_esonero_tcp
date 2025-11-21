#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stddef.h>

#define DEFAULT_SERVER_ADDRESS "127.0.0.1"
#define DEFAULT_SERVER_PORT    56700


#define BUFFER_SIZE 512


typedef struct {
    char type;
    char city[64];
} weather_request_t;


#define STATUS_OK              0
#define STATUS_CITY_NOT_FOUND  1
#define STATUS_INVALID_REQUEST 2


typedef struct {
    unsigned int status;
    char         type;
    float        value;
} weather_response_t;


int  parse_arguments(int argc, char *argv[],
                     char *server_addr, int *port,
                     char *type, char *city, size_t city_size);

int  connect_to_server(const char *server_address, int port);
int  send_weather_request(int socket_desc, const weather_request_t *req);
int  recv_weather_response(int socket_desc, weather_response_t *resp);
void format_and_print_response(const char *server_ip,
                               const weather_response_t *resp,
                               const char *city);

#endif

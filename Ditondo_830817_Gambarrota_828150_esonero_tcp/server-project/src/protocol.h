#ifndef PROTOCOL_H_
#define PROTOCOL_H_


#define DEFAULT_SERVER_PORT 56700


#define BUFFER_SIZE 512
#define QUEUE_SIZE  5


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


float get_temperature(void);
float get_humidity(void);
float get_wind(void);
float get_pressure(void);

int  handle_client(int client_socket, struct sockaddr_in *client_addr);
int  is_supported_city(const char *city);
int  validate_request(const weather_request_t *req);
void build_response(const weather_request_t *req, weather_response_t *resp);

#endif

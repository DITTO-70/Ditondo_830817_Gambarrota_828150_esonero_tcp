

#if defined WIN32
#include <winsock.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif
#include <stdio.h>
#include <stdlib.h>

#if defined WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#define closesocket close
#endif

#define NO_ERROR 0

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

int main(int argc, char *argv[]) {

#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    int my_socket = 0;

    printf("Server terminated.\n");

    closesocket(my_socket);
    clearwinsock();
    return 0;
}

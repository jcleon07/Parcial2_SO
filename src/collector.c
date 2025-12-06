#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "host.h"

#define PORT 8080

int main() {

    struct HostInfo hosts[4];
    pthread_mutex_t lock;


    int fd, r;
    struct sockaddr_in server;

    //Configuracion del server
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT); 
    server.sin_addr.s_addr = INADDR_ANY;

    //Creacion del socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0){
            perror("Error al crear el socket");
            exit(-1);
        }

    //Asigna el socket
    r = bind(fd, (struct sockaddr*)&server, sizeof(server));
        if (r < 0){
            perror("Error al crear el bind");
            close(fd);
            exit(-1);
        }


    return 0;
}
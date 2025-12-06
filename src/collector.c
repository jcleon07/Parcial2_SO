#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "host.h"

int main(int PORT) {

    struct HostInfo hosts[4];
    pthread_mutex_t lock;


    int fd, fd2, r;
    struct sockaddr_in server;

    //Creacion del socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0){
            perror("Error al crear el socket");
            exit(-1);
        }

    //Configuracion del server
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT); 
    server.sin_addr.s_addr = INADDR_ANY;

    //Asigna el socket
    r = bind(fd, (struct sockaddr*)&server, sizeof(server));
        if (r < 0){
            perror("Error al crear el bind");
            close(fd);
            exit(-1);
        }

    r = listen(fd, 4);
        if (r < 0) {
            perror("Error en el listen");
            close(fd);
            exit(-1);
        }

    printf("Servidor encendido");

    fd2 = accept(fd, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
        if (fd2 < 0) {
            perror("Error en el accept");
            close(fd);
            close(fd2);
            exit(-1);
        }

    pthread_create(&hilo, NULL, rec_datos, &fd2);

    
    void *rec_datos(void *arg) {
        int fd_datos = *(int*)arg;
        char buffer[256];

        while (1) {
            int n = recv (fd_datos, buffer, sizeof(buffer)-1, 0)
            if (n <= 0) break;

            buffer[n] = '\0';
            proc_linea[buffer];
        }
    close(fd);
    return NULL;
    
    }


    void proc_linea(char *linea) {
        char tipo[8], ip[32];
        float a, b, c, d;

        pthread_mutex_lock(&lock);

        if(strncmp(linea, "MEM;", 4) == 0) {
            sscanf(linea, "MEM;%31[^;];%f;%f;%f;%f", ip, &a, &b, &c, &d);

            int idx = buscar_host(ip);
            hosts[idx].mem_used_mb = a;
            hosts[idx].mem_free_MB = b;
            hosts[idx].swap_total_mb = c;
            hosts[idx].swap_free_mb = d;
            hosts[idx].last_update = time(NULL);
        }
        else if(strcmp(liena, "CPU;", 4) == 0) {
            sscanf(linea, "MEM%31[^;];%f;%f;%f;%f", ip, &a, &b, &c, &d);

            int idx = buscar_host(ip);
            hosts[idx].cpu_usage = a;
            hosts[idx].cpu_user = b;
            hosts[idx].cpu_system = c;
            hosts[idx].cpu_idle = d;
            hosts[idx].last_update = time(NULL);
        }
    
        pthread_mutex_unlock(&lock);
    }

    int buscar_host(const char *ip){
        for (int i = 0; i < 4, i++) {
            if (strncmp(hosts[1].ip, ip) == 0)
            return i;
        }
        
        for (i = 0, i < 4, i ++){
            if (hosts[i].ip[0] == '\0'){
                strcpy(hosts[i].ip, ip);
                return i;
            }
        }
        return 0;
    }

    return 0;
}
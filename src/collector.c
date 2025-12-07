#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "host.h"


struct HostInfo hosts[4];
pthread_mutex_t lock;

//Inicializacion del arreglo de hosts
 void init_hosts(){
    for (int i = 0; i < 4; i++)
        memset(&hosts[i], 0, sizeof(struct HostInfo));
 }

//Buscar host por IP
int buscar_host(const char *ip){
    for (int i = 0; i < 4; i++) {
        if (strcmp(hosts[1].ip, ip) == 0)
        return i;
    }
    
    for (int i = 0; i < 4; i ++){
        if (hosts[i].ip[0] == '\0'){
            strcpy(hosts[i].ip, ip);
            return i;
        }
    }
    return 0;
}


//Hilo que procesa linea enviada por un agente
void proc_linea(char *linea) {
    char ip[32];
    float a, b, c, d;

    pthread_mutex_lock(&lock);

    if(strncmp(linea, "MEM;", 4) == 0) {
        sscanf(linea, "MEM;%31[^;];%f;%f;%f;%f", ip, &a, &b, &c, &d);

        int idx = buscar_host(ip);
        hosts[idx].mem_used_mb = a;
        hosts[idx].mem_free_mb = b;
        hosts[idx].swap_total_mb = c;
        hosts[idx].swap_free_mb = d;
        hosts[idx].last_update = time(NULL);
    }
    else if(strncmp(linea, "CPU;", 4) == 0) {
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

//Para manejar un cliente
void *rec_datos(void *arg) {
    int fd_datos = *(int*)arg;
    char buffer[256];

    while (1) {
        int n = recv (fd_datos, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;

        buffer[n] = '\0';
        proc_linea(buffer);
    }

close(fd_datos);
return NULL;

}




void iniciar_server(int port){
    int fd, r;
    int *fd2;
    struct sockaddr_in server;

    fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0){
            perror("Error al crear el socket");
            exit(-1);
        }

    //Configuracion del server
    server.sin_family = AF_INET;
    server.sin_port = htons(port); 
    server.sin_addr.s_addr = INADDR_ANY;

    //Asigna el socket
    r = bind(fd, (struct sockaddr*)&server, sizeof(server));
        if (r < 0){
            perror("Error al crear el bind");
            close(fd);
            exit(-1);
        }

    //Poner socket en modo escucha
    r = listen(fd, 4);
        if (r < 0) {
            perror("Error en el listen");
            close(fd);
            exit(-1);
        }

    printf("Servidor encendido");

    pthread_mutex_init(&lock, NULL);

    while (1) {
        fd2 = malloc(sizeof(int));
        *fd2 = accept(fd, NULL, NULL);
            if (fd2 < 0) {
            perror("Error en el accept");
            close(fd);
            close(*fd2);
            exit(-1);
    }
        pthread_t t;
        pthread_create(&t, NULL, rec_datos, fd2);
        pthread_detach(t);
    }
}

//viewer
void *hilo_viewer(void *arg){
    while(1) {
        pthread_mutex_lock(&lock);

        system("clear");
        printf("IP              CPU%%  usr  sys  idle   MemUsed  MemFree\n");

        for (int i = 0; i < 4; i++){
            if (hosts[i].ip[0] != '\0'){
                printf("%-15s %5.1f %5.1f %5.1f %5.1f   %8.1f %8.1f\n",
                hosts[i].ip,
                hosts[i].cpu_usage,
                hosts[i].cpu_user,
                hosts[i].cpu_system,
                hosts[i].cpu_idle,
                hosts[i].mem_used_mb,
                hosts[i].mem_free_mb);
            }
        }

        pthread_mutex_unlock(&lock);
        sleep(2);
    }
}




int main(int argc, char *argv[]){
    if (argc < 2) {
        printf("Uso: %s <puerto>\n",argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    init_hosts();

    pthread_t viewer;
    pthread_create(&viewer, NULL, hilo_viewer, NULL);

    iniciar_server(port);

    return 0;
}
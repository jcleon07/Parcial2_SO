#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


typedef struct {
    long mem_total_kb;
    long mem_available_kb;
    long mem_free_kb;
    long swap_total_kb;
    long swap_free_kb;
} AgentMem;


int read_meminfo(AgentMem *m) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "MemTotal: %ld kB", &m->mem_total_kb);
        sscanf(line, "MemAvailable: %ld kB", &m->mem_available_kb);
        sscanf(line, "MemFree: %ld kB", &m->mem_free_kb);
        sscanf(line, "SwapTotal: %ld kB", &m->swap_total_kb);
        sscanf(line, "SwapFree: %ld kB", &m->swap_free_kb);
    }

    fclose(f);
    return 0;
}

//conectar al colector
int connect_to_collector(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: ./agent_mem <ip_recolector> <puerto> <ip_logica>\n");
        return 1;
    }

    char *collector_ip = argv[1];
    int collector_port = atoi(argv[2]);
    char *ip_logica = argv[3];

    int sock = connect_to_collector(collector_ip, collector_port);
    if (sock < 0) {
        printf("No se pudo conectar al colector\n");
        return 1;
    }

    AgentMem m;

    while (1) {
        if (read_meminfo(&m) != 0) {
            printf("Error leyendo /proc/meminfo\n");
            break;
        }

        float mem_used_mb = (m.mem_total_kb - m.mem_available_kb) / 1024.0;
        float mem_free_mb = m.mem_free_kb / 1024.0;
        float swap_total_mb = m.swap_total_kb / 1024.0;
        float swap_free_mb = m.swap_free_kb / 1024.0;

        char line[256];
        sprintf(line,
            "MEM;%s;%.2f;%.2f;%.2f;%.2f\n",
            ip_logica, mem_used_mb, mem_free_mb, swap_total_mb, swap_free_mb
        );

        send(sock, line, strlen(line), 0);

        sleep(1); // espera un segundo
    }

    close(sock);
    return 0;
}

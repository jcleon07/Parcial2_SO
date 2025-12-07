#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef struct 
{
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
} AgentCPU;


int read_cpuinfo(AgentCPU *m) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
    perror("fopen");
    return 1;
}

char cpu_label[5];
char line[256];

while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "%4s %lu %lu %lu %lu %lu %lu %lu %lu", cpu_label, &m->user, &m->nice, &m->system, &m->idle, &m->iowait, &m->irq, &m->softirq, &m->steal) == 9) {
        break;
    }
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


int main(int argc, char *argv[]){
    if (argc != 4){
        printf("Uso: .agent_cpu <ip_recolector> <puerto> <ip_logica>\n");
        return 1;
    }
    char *collector_ip = argv[1];
    int collector_port = atoi (argv[2]);
    char *ip_logica_agente = argv[3];

    int sock = connect_to_collector(collector_ip, collector_port);
    if (sock < 0){
        printf("El agente de cpu no se pudo conectar al colector\n");
        return 1;

    }

    AgentCPU antes, despues;
    
    while (1){
        // Primer lectura de CPU
        if (read_cpuinfo(&antes) != 0){
            printf("Error leyendo /proc/stat\n");
            break;
        }
        
        // Sleep 1 segundo para la siguiente lectura de CPU
        sleep(1);
        
        // Leer CPU segunda vez
        if (read_cpuinfo(&despues) != 0){
            printf("Error leyendo /proc/stat\n");
            break;
        }
        
        // Calcular deltas
        unsigned long delta_user = despues.user - antes.user;
        unsigned long delta_nice = despues.nice - antes.nice;
        unsigned long delta_system = despues.system - antes.system;
        unsigned long delta_idle = despues.idle - antes.idle;
        unsigned long delta_iowait = despues.iowait - antes.iowait;
        unsigned long delta_irq = despues.irq - antes.irq;
        unsigned long delta_softirq = despues.softirq - antes.softirq;
        unsigned long delta_steal = despues.steal - antes.steal;
        
        // Calcular CPU_total (suma de todos los deltas)
        unsigned long cpu_total = delta_user + delta_nice + delta_system + delta_idle 
                                 + delta_iowait + delta_irq + delta_softirq + delta_steal;
        
        // Calcular CPU_idle
        unsigned long cpu_idle = 100.0 * delta_idle / cpu_total;
        
        // Evitar división por cero
        if (cpu_total == 0) {
            cpu_total = 1;
        }
        
        // Calcular porcentaje de uso
        unsigned long cpu_usage = 100.0 * (cpu_total - delta_idle) / cpu_total;

        unsigned long cpu_user = 100.0 * delta_user / cpu_total;
        unsigned long cpu_system = 100.0 * delta_system / cpu_total;
        
        
        //CPU;<ip_logica_agente>;<CPU_usage>;<user_pct>;<system_pct>;<idle_pct>\n
        char line[256];
        sprintf(line, "CPU;%s;%.2f;%.2f;%.2f;%.2f\n", ip_logica_agente, cpu_usage, cpu_user, cpu_system, cpu_idle);
        send(sock, line, strlen(line), 0);
        
        // Sleep antes de siguiente iteración
        sleep(1);
    }
    
    close(sock);
    return 0;
}

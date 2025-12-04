/*
 * agent_mem.c
 * Agente de memoria para el proyecto de monitor distribuido
 *
 * Comportamiento:
 * - Lee /proc/meminfo cada 1 segundo
 * - Extrae: MemTotal, MemAvailable, MemFree, SwapTotal, SwapFree (en kB)
 * - Calcula mem_used_MB = (MemTotal - MemAvailable) / 1024.0
 * - Se conecta por TCP al recolector y envía una línea con formato:
 *     MEM;<ip_logica_agente>;<mem_used_MB>;<MemFree_MB>;<SwapTotal_MB>;<SwapFree_MB>\n
 * - Reconecta en caso de fallo
 * - Maneja señal SIGINT para terminar limpiamente
 *
 * Uso:
 *   ./agent_mem <ip_recolector> <puerto> <ip_logica_agente>
 *
 * Compilar:
 *   gcc -std=c11 -O2 -Wall -Wextra -o agent_mem agent_mem.c
 *
 * Prueba rápida (en la máquina del recolector):
 *   nc -l -p 9000
 * En el agente:
 *   ./agent_mem 127.0.0.1 9000 10.0.0.2
 * Deberías ver líneas que empiezan con "MEM;10.0.0.2;..."
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static volatile sig_atomic_t keep_running = 1;

void int_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

struct meminfo {
    long MemTotal_kb;
    long MemAvailable_kb;
    long MemFree_kb;
    long SwapTotal_kb;
    long SwapFree_kb;
};

// Lee /proc/meminfo y llena la estructura (valores en kB).
// Devuelve 0 si al menos MemTotal fue leído, -1 si hubo error.
int parse_meminfo(struct meminfo *m) {
    if (!m) return -1;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    // Inicializar con -1 para detectar ausencia
    m->MemTotal_kb = -1;
    m->MemAvailable_kb = -1;
    m->MemFree_kb = -1;
    m->SwapTotal_kb = -1;
    m->SwapFree_kb = -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%ld", &m->MemTotal_kb);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%ld", &m->MemAvailable_kb);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line + 8, "%ld", &m->MemFree_kb);
        } else if (strncmp(line, "SwapTotal:", 10) == 0) {
            sscanf(line + 10, "%ld", &m->SwapTotal_kb);
        } else if (strncmp(line, "SwapFree:", 9) == 0) {
            sscanf(line + 9, "%ld", &m->SwapFree_kb);
        }
    }

    fclose(f);
    return (m->MemTotal_kb >= 0) ? 0 : -1;
}

// Conectar al recolector (ip/puerto). Devuelve socket fd o -1 en error.
int connect_to_collector(const char *host, const char *port) {
    struct addrinfo hints, *res, *rp;
    int sfd = -1;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;

        // Opcional: timeout en connect (dejar bloqueante simple es suficiente para el laboratorio)
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // conectado
            break;
        }
        close(sfd);
        sfd = -1;
    }

    freeaddrinfo(res);
    return sfd; // -1 si no conectó
}

// Envía todos los bytes del buffer (maneja envíos parciales). Retorna 0 éxito, -1 error.
int send_all(int sfd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sfd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            if (n == -1 && errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <ip_recolector> <puerto> <ip_logica_agente>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *collector_ip = argv[1];
    const char *collector_port = argv[2];
    const char *logical_ip = argv[3];

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    int sock = -1;
    int reconnect_delay = 1; // segundos, backoff simple

    while (keep_running) {
        struct meminfo m;
        if (parse_meminfo(&m) != 0) {
            fprintf(stderr, "Error leyendo /proc/meminfo\n");
            sleep(1);
            continue;
        }

        double mem_used_mb = 0.0;
        double memfree_mb = 0.0;
        double swaptotal_mb = 0.0;
        double swapfree_mb = 0.0;

        if (m.MemTotal_kb >= 0 && m.MemAvailable_kb >= 0) {
            mem_used_mb = ((double)(m.MemTotal_kb - m.MemAvailable_kb)) / 1024.0;
        }
        if (m.MemFree_kb >= 0) memfree_mb = ((double)m.MemFree_kb) / 1024.0;
        if (m.SwapTotal_kb >= 0) swaptotal_mb = ((double)m.SwapTotal_kb) / 1024.0;
        if (m.SwapFree_kb >= 0) swapfree_mb = ((double)m.SwapFree_kb) / 1024.0;

        // Construir la línea a enviar
        char outbuf[256];
        int n = snprintf(outbuf, sizeof(outbuf), "MEM;%s;%.2f;%.2f;%.2f;%.2f\n",
                         logical_ip, mem_used_mb, memfree_mb, swaptotal_mb, swapfree_mb);
        if (n < 0 || n >= (int)sizeof(outbuf)) {
            fprintf(stderr, "Buffer overflow preparando línea (improbable)\n");
            // no salir, saltar
            sleep(1);
            continue;
        }

        // Asegurar conexión
        if (sock < 0) {
            sock = connect_to_collector(collector_ip, collector_port);
            if (sock < 0) {
                fprintf(stderr, "No pude conectar con %s:%s - reintentando en %d s\n",
                        collector_ip, collector_port, reconnect_delay);
                sleep(reconnect_delay);
                if (reconnect_delay < 16) reconnect_delay *= 2;
                continue;
            }
            // Conectado
            reconnect_delay = 1;
            fprintf(stderr, "Conectado a %s:%s\n", collector_ip, collector_port);
        }

        // Enviar
        if (send_all(sock, outbuf, (size_t)strlen(outbuf)) != 0) {
            fprintf(stderr, "Error enviando, cierro socket y reintentar\n");
            close(sock);
            sock = -1;
            // no dormir extra: el bucle duerme al final
        } else {
            // Mensaje enviado correctamente
            // Si quieres traza detallada, descomenta la siguiente línea:
            // fprintf(stderr, "Enviado: %s", outbuf);
        }

        // Esperar 1 segundo antes de la siguiente lectura/envío
        // Usamos nanosleep para poder interrumpir con señales de forma razonable.
        struct timespec ts = {.tv_sec = 1, .tv_nsec = 0};
        while (nanosleep(&ts, &ts) == -1 && errno == EINTR && keep_running) {
            // continuar durmiendo el tiempo restante
        }
    }

    if (sock >= 0) close(sock);
    fprintf(stderr, "Agente finalizado correctamente\n");
    return EXIT_SUCCESS;
}

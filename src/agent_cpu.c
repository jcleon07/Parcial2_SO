#include <stdio.h>


int main(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
    perror("fopen");
    return 1;
}



char cpu_label[5];
unsigned long user, nice, system, idle, iowait, irq, softirq, steal;


if(fscanf(f, "%4s %lu %lu %lu %lu %lu %lu %lu %lu",cpu_label,&user, &nice, &system, &idle,&iowait, &irq, &softirq, &steal) == 9) {

printf("Label: %s\n", cpu_label);
printf("user=%lu nice=%lu system=%lu idle=%lu\n",user, nice, system, idle);
} else {
fprintf(stderr, "No se pudo leer la línea de cpu\n");
}


fclose(f);
return 0;
}
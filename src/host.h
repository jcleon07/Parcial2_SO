#ifndef HOST_H
#define HOST_H

struct HostInfo {
    char ip[32];
    float cpu_usage;
    float cpu_user;
    float cpu_system;
    float cpu_idle;
    float mem_used_mb;
    float mem_free_mb;
    float swap_total_mb;
    float swap_free_mb;
    time_t last_update;
};

#endif
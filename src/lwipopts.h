#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// --- Обязательные настройки для FreeRTOS ---
#define NO_SYS                  0
#define SYS_LIGHTWEIGHT_PROT    1
#define LWIP_NETCONN            0           // не используем netconn API
#define LWIP_SOCKET             0

// --- Память (жёстко, но должно работать) ---
#define MEM_ALIGNMENT           4
#define MEM_SIZE                (16*1024)    // 16 КБ кучи
#define MEMP_NUM_PBUF           8
#define PBUF_POOL_SIZE          16
#define PBUF_POOL_BUFSIZE       1524        // полноразмерный буфер

// --- Количество структур для протоколов (минимально) ---
#define MEMP_NUM_UDP_PCB        4
#define MEMP_NUM_TCP_PCB        0
#define MEMP_NUM_TCP_PCB_LISTEN 0
#define MEMP_NUM_TCP_SEG        0
#define MEMP_NUM_SYS_TIMEOUT    6
#define MEMP_NUM_ARP_QUEUE      5
#define MEMP_NUM_ARP_TABLE      10

// --- Включённые протоколы ---
#define LWIP_TCP                0           // отключаем TCP для экономии
#define LWIP_UDP                1           // для DHCP (если нужно) или теста
#define LWIP_ICMP               1
#define LWIP_DHCP               0           // статический IP проще
#define LWIP_RAW                1           // для ICMP

// --- ARP (обязательно для Ethernet) ---
#define LWIP_ARP                1

// --- Таймеры ---
#define LWIP_TIMERS             1
#define LWIP_SYS_TIMEOUTS       1

// --- Остальное ---
#define LWIP_STATS              0
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_PROVIDE_ERRNO      1
#define LWIP_ERRNO_STDC         0

#endif
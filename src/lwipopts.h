// lwipopts.h -- содержит настройки стека LwIP

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                  0           // lwip работает с операционной системой
#define SYS_LIGHTWEIGHT_PROT    1           // включаем защиту критических секций
#define LWIP_NETCONN            0           // не используем netconn API
#define LWIP_SOCKET             0           // отключаем BSD-подобный интерфейс сокетов

#define MEM_ALIGNMENT           4           // выравниваем выделенную память в куче LWIP
#define MEM_SIZE                (16*1024)   // 16 КБ кучи
#define MEMP_NUM_PBUF           8           // количество структур pbuf
#define PBUF_POOL_SIZE          16          // размер пула буферов
#define PBUF_POOL_BUFSIZE       1524        // полноразмерный буфер

#define MEMP_NUM_UDP_PCB        4           // максимальное кол-во одновременно активных блоков UDP
#define MEMP_NUM_TCP_PCB        0           // отключаем поддержку TCP
#define MEMP_NUM_TCP_PCB_LISTEN 0
#define MEMP_NUM_TCP_SEG        0
#define MEMP_NUM_SYS_TIMEOUT    6           // количество системных тайм-аутов
#define MEMP_NUM_ARP_QUEUE      5           // количество пакетов в очереди 
#define MEMP_NUM_ARP_TABLE      10          // размер таблицы ARP

#define LWIP_TCP                0           // отключаем TCP для экономии
#define LWIP_UDP                1           // для DHCP (если нужно) или теста
#define LWIP_ICMP               1           // включаем ICMP для ответа на ping
#define LWIP_DHCP               0           // статический IP
#define LWIP_RAW                1           // включаем RAW сокеты для ICMP

#define LWIP_ARP                1           // включаем ARP для преобразования IP в MAC

#define LWIP_TIMERS             1           // включаем системные таймеры
#define LWIP_SYS_TIMEOUTS       1           // включаем тайм-ауты на уровне ОС

#define LWIP_STATS              0           // отключаем сбор статистики
#define LWIP_NETIF_LINK_CALLBACK 1          // включаем коллбэк при изменении состояния кабеля
#define LWIP_PROVIDE_ERRNO      1           // предоставляем переменную errno
#define LWIP_ERRNO_STDC         0           // при 0 используется внутренняя реализация errno LwIP

#endif
#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

// инициализируем аппаратуру на низком уровне
void low_level_init(struct netif *netif);

// отправляем пакет
err_t low_level_output(struct netif *netif, struct pbuf *p);

// принимаем пакет из dma в pbuf
struct pbuf *low_level_input(struct netif *netif);

// инициализируем интерфейс
err_t ethernetif_init(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNETIF_H */
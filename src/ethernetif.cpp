// ethernetif.cpp -- данный файл использует библиотека lwip
// в нем определяются функции для низкоуровневого доступа
// к микросхеме lan8720

extern "C" {
    #include "lwip/netif.h"
    #include "lwip/etharp.h"
    #include "lwip/pbuf.h"
    #include "lwip/tcpip.h"
}

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/ethernet/mac.h>
#include <libopencm3/ethernet/phy.h>
#include <libopencm3/cm3/nvic.h>
#include "netif/ethernet.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "ethernetif.h"
#include "lan8720_config.hpp"

#include <cstring>
#include <cstdio>

// конфигурация драйвера
#define ETH_RX_DESC_CNT     2
#define ETH_TX_DESC_CNT     16
#define ETH_RX_BUF_SIZE     1524
#define ETH_TX_BUF_SIZE     1524

// устанавливаем размер дескриптора
#define ETH_DESC_SIZE       16

// общий размер буфера
#define ETH_BUFFER_SIZE  ((ETH_TX_DESC_CNT + ETH_RX_DESC_CNT) * ETH_DESC_SIZE + \
                          ETH_TX_DESC_CNT * ETH_TX_BUF_SIZE + \
                          ETH_RX_DESC_CNT * ETH_RX_BUF_SIZE)

// выровненный общий буфер
static __attribute__((aligned(4))) uint8_t eth_dma_buffer[ETH_BUFFER_SIZE];

// инициализируем аппаратуру на низком уровне
extern "C" void low_level_init(struct netif *netif)
{
    // создаем объект драйвера
    Lan8720Config phy;
    phy.lan8720Reset(0);
    // включаем режим автоопределения
    phy.lan8720AutoNegEnable(0);
    // ждем установления линка (5 секунд)
    phy.lan8720WaitLink(0, 5000000);

    // инициализируем MAC/DMA
    eth_init(0, ETH_CLK_150_168MHZ);
    eth_set_mac(netif->hwaddr);

    // инициализируем дескрипторы
    // дескрипторы действуют как указатели на буферы
    eth_desc_init(eth_dma_buffer,           // указываем область памяти 
                  ETH_TX_DESC_CNT,          // количество передаваемых дескрипторов 
                  ETH_RX_DESC_CNT,          // количество получаемых дескрипторов
                  ETH_TX_BUF_SIZE,          // кол-во байтов в каждом буфере передачи 
                  ETH_RX_BUF_SIZE,          // кол-во байтов в каждом буфере приема 
                  false);                   // расширенные дескрипторы отключены

    // заполняем netif
    // максимальный размер пакета данных
    netif->mtu          = 1500;    
    // заполняем флаги
    // широковещание вкл                                  
    netif->flags        = NETIF_FLAG_BROADCAST |    
    // ARP (Address Resolution Protocol) вкл
                        NETIF_FLAG_ETHARP | 
    // связь установлена
                        NETIF_FLAG_LINK_UP;
    // инициализируем функции
    netif->input = ethernet_input;
    // вызывается ip_output, когда пакет готов к передаче
    netif->output       = etharp_output;
    // вызывается, когда необработанный пакет канала готов к передаче
    netif->linkoutput   = low_level_output;

    // запуск mac и dma (сначала запускаем DMA, потом настраиваем прерывания)
    eth_start();
}

// модифицируем функцию eth_rx из libopencm3
// добавим сброс ошибок и перезапуск dma
bool safe_eth_tx(uint8_t *data, uint32_t len) {
    // сбрасываем ошибки dma передачи
    // ETH_DMASR -- регистр статуса dma
    ETH_DMASR = ETH_DMASR_TBUS |    // сбрасываем бит TBUS, если дескриптор вдруг занят CPU
                ETH_DMASR_TUS |     // снимает бит переполнения буфера передачи
                ETH_DMASR_TJTS;     // сбрасываем бит TJTS, если вдруг возникла ошибка
                                    // в передатчике ethernet

    ETH_DMAOMR |= ETH_DMAOMR_FTF;   // сброс FIFO передатчика
    ETH_DMATPDR = 0;                // запрос на передачу, чтобы разбудить DMA

    // отправляем пакет через стандартную функцию
    if (!eth_tx(data, len)) {
        // если не вышло, попробуем перезапустить DMA
        ETH_DMAOMR |= ETH_DMAOMR_FTF;
        ETH_DMATPDR = 0;
        return false;
    }
    return true;
}

// отправляем пакет
extern "C" err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    // выводим отладочное сообщение
    usart_send_blocking(USART2, 'T');

    // копируем цепочку pbuf в линейный массив
    static __attribute__((aligned(4))) uint8_t tx_buffer[ETH_TX_BUF_SIZE];
    uint16_t total_len = 0;
    struct pbuf *q;

    for (q = p; q != NULL; q = q->next) {
        if (total_len + q->len > ETH_TX_BUF_SIZE) {
            return ERR_BUF;
        }
        memcpy(tx_buffer + total_len, q->payload, q->len);
        total_len += q->len;
    }

    if (total_len < 60) {
        memset(tx_buffer + total_len, 0, 60 - total_len);
        total_len = 60;
    }

    if (!safe_eth_tx(tx_buffer, total_len)) {
        usart_send_blocking(USART2, 'E');
        return ERR_BUF;
    } else {
        usart_send_blocking(USART2, 'O');
    }
    
    return ERR_OK;
}

// принимаем пакет из dma в pbuf
extern "C" struct pbuf *low_level_input(struct netif *netif)
{
    uint8_t dummy_buf[ETH_RX_BUF_SIZE];
    uint32_t len = 0;
    bool res = eth_rx(dummy_buf, &len, ETH_RX_BUF_SIZE);
    if (!res) return NULL;

    // отправляем букву р, когда пакет принят
    // usart_send_blocking(USART2, 'R');

    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p != NULL) {
        memcpy(p->payload, dummy_buf, len);
    }
    
    return p;   // возвращаем p, а вызов netif->input будет в задаче
}

// инициализируем интерфейс
extern "C" err_t ethernetif_init(struct netif *netif)
{
    // устанавливаем mac адрес
    netif->hwaddr_len = 6;
    netif->hwaddr[0] = 0x02;
    netif->hwaddr[1] = 0x00;
    netif->hwaddr[2] = 0x00;
    netif->hwaddr[3] = 0x12;
    netif->hwaddr[4] = 0x34;
    netif->hwaddr[5] = 0x56;

    // передаем структуру и инициализируем аппаратуру
    low_level_init(netif);

    return ERR_OK;
}
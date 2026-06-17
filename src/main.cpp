#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "lwip/tcpip.h"
#include "lwip/udp.h"

#include <libopencm3/stm32/rcc.h> 
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/ethernet/mac.h>
#include <libopencm3/ethernet/phy.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>

#include <cstdio>
#include <string.h>

#include "setup.hpp"
#include "adns3080.hpp"
#include "lan8720_config.hpp"
#include "ethernetif.h"

// конфигурируем периферию
SetupPeriph peripheral = SetupPeriph();

// создаем объект конфгурации lan8720
Lan8720Config lan8720 = Lan8720Config();

// создадим структуру с наименованиями выводов левого датчика
const ADNS3080::Adns3080Pins l_camera_pins = {
    .cs_gpio_port = GPIOA,      
    .cs_gpio_pin = GPIO4,
    .spi = SPI1,
    .reset_gpio_port = GPIOB,
    .reset_gpio_pin = GPIO0,
    .dma = DMA2,
    .stream_tx = DMA_STREAM3,
    .stream_rx = DMA_STREAM2
};

// создадим структуру с наименованиями выводов правого датчика
const ADNS3080::Adns3080Pins r_camera_pins = {
    .cs_gpio_port = GPIOD,      
    .cs_gpio_pin = GPIO0,
    .spi = SPI3,
    .reset_gpio_port = GPIOD,
    .reset_gpio_pin = GPIO1,
    .dma = DMA1,
    .stream_tx = DMA_STREAM5,
    .stream_rx = DMA_STREAM0
};

// объекты камер
ADNS3080 l_camera = ADNS3080(l_camera_pins);
ADNS3080 r_camera = ADNS3080(r_camera_pins);

// структура данных о перемещении
ADNS3080::MotionData l_data, r_data;

// создадим выровненную структуру с данными для отправки
#pragma pack(push, 1)
struct SensorPacket {
    // левый датчик
    uint8_t  l_motion;
    int8_t   l_dx;
    int8_t   l_dy;
    uint8_t  l_squal;
    uint16_t l_shutter;
    uint8_t  l_max_pix;
    // правый датчик
    uint8_t  r_motion;
    int8_t   r_dx;
    int8_t   r_dy;
    uint8_t  r_squal;
    uint16_t r_shutter;
    uint8_t  r_max_pix;
} pkt;
// возвращаем выравнивание
#pragma pack(pop)

static SemaphoreHandle_t uart_tx_semaphore = NULL;

// создаем глобальный буффер для строки
char buffer[128];
uint16_t id1, id2;

// создаем задачу
void uart_ts_task(void *pvParameters)
{
    // создаем бинарный семафор (начальное значение 0)
    uart_tx_semaphore = xSemaphoreCreateBinary();
    // освобождаем семафор (прибавляем 1)
    xSemaphoreGive(uart_tx_semaphore);
   
    while (1) {
        if (xSemaphoreTake(uart_tx_semaphore, portMAX_DELAY) == pdTRUE) {
            // формируем строку
            uint32_t str = 
                sprintf(buffer, 
                        "L - M: %d, X: %4d, Y: %4d, SQ: %3u, SH: %d, MP: %u\r\nR - M: %d, X: %4d, Y: %4d, SQ: %3u, SH: %d, MP: %u\r\n", 
                        l_data.motion, l_data.dx, l_data.dy, 
                        l_data.squal, l_data.shutter, l_data.max_pix,
                        r_data.motion, r_data.dx, r_data.dy, 
                        r_data.squal, r_data.shutter, r_data.max_pix);

            // настраиваем адреса и запускаем канал DMA
            dma_set_memory_address(DMA1, DMA_STREAM6, (uint32_t)buffer);
            dma_set_number_of_data(DMA1, DMA_STREAM6, str);
            dma_enable_stream(DMA1, DMA_STREAM6);

            vTaskDelay(pdMS_TO_TICKS(1000));
        } 
    }
}

// обработчик прерывания usart2_tx
extern "C"
void dma1_stream6_isr()
{
    // проверяем, сработал ли флаг прерывания
    if (dma_get_interrupt_flag(DMA1, DMA_STREAM6, DMA_TCIF)) {
        // сбрасываем флаг
        dma_clear_interrupt_flags(DMA1, DMA_STREAM6, DMA_TCIF);
        // отключаем канал, чтобы он не пытался передавать снова
        dma_disable_stream(DMA1, DMA_STREAM6);

        // освобождаем семафор и отдаем его задаче
        BaseType_t woken = pdFALSE;
        // даём семафор из ISR
        xSemaphoreGiveFromISR(uart_tx_semaphore, &woken);
        // смотрим флаг woken
        portYIELD_FROM_ISR(woken);
    }
}

// определяем семафоры для работы с левым датчиком
static SemaphoreHandle_t l_dma_tx_semaphore = NULL;
static SemaphoreHandle_t l_dma_rx_semaphore = NULL;

// создаем задачу для отправки данных датчику
void l_motionBurst_task(void *pvParameters)
{
    // создаем семафоры
    l_dma_tx_semaphore = xSemaphoreCreateBinary();
    l_dma_rx_semaphore = xSemaphoreCreateBinary();

    const TickType_t period = pdMS_TO_TICKS(5); // опрос каждые 5 мс
    // const TickType_t period = pdMS_TO_TICKS(0.5); // опрос каждые 500 мкс

    for (;;) {
        l_camera.motionBurstDma(l_data, l_dma_tx_semaphore, l_dma_rx_semaphore);
        vTaskDelay(period);
    }
}

// обработчик прерывания spi1_tx
extern "C"
void dma2_stream3_isr()
{
    if (dma_get_interrupt_flag(DMA2, DMA_STREAM3, DMA_TCIF)) {
        dma_clear_interrupt_flags(DMA2, DMA_STREAM3, DMA_TCIF);

        // освобождаем семафор и отдаем его задаче
        BaseType_t woken = pdFALSE;
        // даём семафор из ISR
        xSemaphoreGiveFromISR(l_dma_tx_semaphore, &woken);
        // смотрим флаг woken
        portYIELD_FROM_ISR(woken);
    }
}

// обработчик прерывания spi1_rx
extern "C"
void dma2_stream2_isr()
{
    if (dma_get_interrupt_flag(DMA2, DMA_STREAM2, DMA_TCIF)) {
        dma_clear_interrupt_flags(DMA2, DMA_STREAM2, DMA_TCIF);

        // освобождаем семафор
        // освобождаем семафор и отдаем его задаче
        BaseType_t woken = pdFALSE;

        // даём семафор из ISR
        xSemaphoreGiveFromISR(l_dma_rx_semaphore, &woken);

        // смотрим флаг woken
        portYIELD_FROM_ISR(woken);
    }
}

// определяем семафоры для работы с правым датчиком
static SemaphoreHandle_t r_dma_tx_semaphore = NULL;
static SemaphoreHandle_t r_dma_rx_semaphore = NULL;

// создаем задачу для отправки данных датчику
void r_motionBurst_task(void *pvParameters)
{
    // создаем семафоры
    r_dma_tx_semaphore = xSemaphoreCreateBinary();
    r_dma_rx_semaphore = xSemaphoreCreateBinary();

    const TickType_t period = pdMS_TO_TICKS(5); // опрос каждые 5 мс
    // const TickType_t period = pdMS_TO_TICKS(0.5); // опрос каждые 500 мКс

    for (;;) {
        r_camera.motionBurstDma(r_data, r_dma_tx_semaphore, r_dma_rx_semaphore);
        vTaskDelay(period);
    }
}

// обработчик прерывания spi3_tx
extern "C"
void dma1_stream5_isr()
{
    if (dma_get_interrupt_flag(DMA1, DMA_STREAM5, DMA_TCIF)) {
        dma_clear_interrupt_flags(DMA1, DMA_STREAM5, DMA_TCIF);

        // освобождаем семафор и отдаем его задаче
        BaseType_t woken = pdFALSE;
        // даём семафор из ISR
        xSemaphoreGiveFromISR(r_dma_tx_semaphore, &woken);
        // смотрим флаг woken
        portYIELD_FROM_ISR(woken);
    }
}

// обработчик прерывания spi3_rx
extern "C"
void dma1_stream0_isr()
{
    if (dma_get_interrupt_flag(DMA1, DMA_STREAM0, DMA_TCIF)) {
        dma_clear_interrupt_flags(DMA1, DMA_STREAM0, DMA_TCIF);

        // освобождаем семафор и отдаем его задаче
        BaseType_t woken = pdFALSE;

        // даём семафор из ISR
        xSemaphoreGiveFromISR(r_dma_rx_semaphore, &woken);

        // смотрим флаг woken
        portYIELD_FROM_ISR(woken);
    }
}

// задача опроса
static void polling_rx_task(void *arg) {
    // извлекаем указатель на структуру сетевого интерфейса
    struct netif *netif = (struct netif*)arg;
    // внешний цикл -- задача работает постоянно
    while (1) {
        // внутренний цикл -- читаем все накопившиеся пакеты, пока они есть
        while (1) {
            // вызываем low_level_input() и читаем пакеты из dma в pbuf
            struct pbuf *p = low_level_input(netif);

            // если пакетов больше нет, выходим из цикла
            if (p == NULL)
                break; 
            
            // если пакеты есть, передаем их в стек tcp/ip
            err_t err = netif->input(p, netif);
            // если стек не принял пакет, освобождаем его вручную
            if (err != ERR_OK)
                pbuf_free(p);
        }
        // опрашиваем каждые 5 мс
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// создаем задачу, которая инициализирует и поддерживает сетевой стек lwip
void network_task(void *arg) {
    // запускаем внутреннюю системную задачу lwip
    // задача обрабатывает все пакеты, таймеры,
    // очереди сообщений и вызовы api
    tcpip_init(NULL, NULL);
    
    // создаем объект интерфейса ethernet netif
    struct netif netif;
    // создаем ip адрес, маску и шлюз
    ip4_addr_t ip, mask, gw;
    IP4_ADDR(&ip, 192,168,1,100);
    IP4_ADDR(&mask, 255,255,255,0);
    IP4_ADDR(&gw, 192,168,1,1);
    
    // регистрируем интерфейс в lwip
    // передаем параметры и указатели на функции
    netif_add(&netif, &ip, &mask, &gw, NULL, ethernetif_init, tcpip_input);
    // устанавливаем интерфейс по умолчанию
    netif_set_default(&netif);
    // активируем интерфейс
    netif_set_up(&netif);
    netif_set_link_up(&netif);
    
    // создаем задачу
    xTaskCreate(polling_rx_task, "poll_rx", 2048, &netif, 3, NULL);
    
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

// создадим функцию отправки данных по udp
static void udp_send_task(void *arg)
{
    // ждем 1 с для инициализации стека
    vTaskDelay(pdMS_TO_TICKS(1000));

    // указатель на блок управления udp
    struct udp_pcb *udp_pcb;
    ip_addr_t remote_ip;
    err_t err;

    // создаем блок управления udp
    udp_pcb = udp_new();
    // проверяем
    if (udp_pcb == NULL) {
        while (1);
    }

    // задаем ip адрес получателя
    IP4_ADDR(&remote_ip, 192,168,1,50);

    // бесконечный цикл отправки
    while (1) {
        // заполняем и отправляем структуру
        // левый датчик
        pkt.l_motion = l_data.motion;
        // приводим значения смещения к системе
        // координат робота
        pkt.l_dx = -l_data.dx;
        pkt.l_dy = -l_data.dy;
        pkt.l_squal = l_data.squal;
        pkt.l_shutter = l_data.shutter;
        pkt.l_max_pix = l_data.max_pix;
        // правый датчик
        pkt.r_motion = r_data.motion;
        // аналогично
        pkt.r_dx = r_data.dx;
        pkt.r_dy = r_data.dy;
        pkt.r_squal = r_data.squal;
        pkt.r_shutter = r_data.shutter;
        pkt.r_max_pix = r_data.max_pix;

        // создаем пакетный буффер для отправки
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(pkt), PBUF_RAM);
        // проверяем
        if (p != NULL) {
            memcpy(p->payload, &pkt, sizeof(pkt));
            // отправляем udp датаграмму на порт 8888
            err = udp_sendto(udp_pcb, p, &remote_ip, 8888);
            pbuf_free(p);
            // проверяем
            if (err != ERR_OK) {
                // выводим отладочное сообщение
                usart_send_blocking(USART2, 'E');
            }
            
        }
        // отправляем данные с периодом 20 мс
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

int main () { 
    // небольшая задержка для включения датчика
    for (volatile uint32_t i = 0; i < 2000000; i++);
    
    // сконфигурируем датчики
    // включаем подсветку и устанавливаем высокое разрешение
    volatile uint8_t l_setup = l_camera.setup(true, true);
    l_camera.delayUs(ADNS3080_T_SWW);
    volatile uint8_t r_setup = r_camera.setup(true, true);
	r_camera.delayUs(ADNS3080_T_SWW);

    // установим динамический фреймрейт для обеих камер
    volatile uint8_t l_ex_setup = l_camera.extendedSetup(false);
    l_camera.delayUs(ADNS3080_T_SWW);
    volatile uint8_t r_ex_setup = r_camera.extendedSetup(false);
    l_camera.delayUs(ADNS3080_T_SWW);

    // // проверяем корректность подключения
    // if (l_setup == true && r_setup == true && l_ex_setup == true && r_ex_setup == true) {
    //     usart_send_blocking(USART2, 't');
    // } else { 
    //     usart_send_blocking(USART2, 'f');
    // }

    // сконфигурируем lan8720
    // прочитаем id
    // uint8_t lan8720_addr = 0;
    // id1 = lan8720.lan8720ReadPhyId1(lan8720_addr);
    // id2 = lan8720.lan8720ReadPhyId2(lan8720_addr);

    xTaskCreate(&l_motionBurst_task, "l_motionBurst", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
    xTaskCreate(&r_motionBurst_task, "r_motionBurst", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
    // xTaskCreate(&uart_ts_task, "usart2", 1024, NULL, 1, NULL);
    xTaskCreate(&network_task, "network_task", 2048, NULL, 2, NULL);
    xTaskCreate(&udp_send_task, "udp_send", 1024, NULL, 3, NULL);
    
    vTaskStartScheduler();
}

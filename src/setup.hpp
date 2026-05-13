#include <libopencm3/stm32/gpio.h>
/* Хэдэр для конфигурации микроконтроллера */

constexpr uint32_t BAUD_SPEED = 115200;  // скорость передачи данных
constexpr uint8_t WORD_SIZE = 8;

constexpr uint16_t T_nRSTIA = 400;  // время удержания nRST в активном состоянии (0), мкс
constexpr uint16_t T_AFTERnRST = 5000; // время удержания nRST в активном состоянии (0), мкс

// создадим класс для инициализации необходимой периферии мк
class SetupPeriph 
{	 
public:
    // конструктор с конфигурацией
    SetupPeriph() 
    {
        clockSetup();
        timerSetup();
        macSetup();
        adns3080PinsSetup();
        spi1Setup();
        spi1TxDmaSetup();
        spi1RxDmaSetup();
        spi3Setup();
        spi3TxDmaSetup();
        spi3RxDmaSetup();
        usart2Setup();
        usart2DmaSetup();
    }

    // тактирование периферии
    void clockSetup();

    // конфигурация таймера TIM6
    void timerSetup();

    // временная задержка (мкс)
    void delayUs(uint16_t delay);

    // перезагружаем PHY
    void resetPhy();

    // конфигурируем порты MAC контроллера
    void macGpioSetup();

    // конфигурация MAC контроллера
    void macSetup();

    // конфигурируем вспомогательные выводы датчика
    void adns3080PinsSetup();

    // конфигурируем SPI для двух датчиков
    void spi1Setup();

    // конфигурируем dma для spi1_tx
    void spi1TxDmaSetup();

    // конфигурируем dma для spi1_rx
    void spi1RxDmaSetup();

    void spi3Setup();

    // конфигурируем DMA для spi3_tx
    void spi3TxDmaSetup();

    // конфигурируем DMA для spi3_rx
    void spi3RxDmaSetup();

    // конфигурируем dma1, канал 4, поток 6
    void usart2DmaSetup();
    
    void usart2Setup();
};
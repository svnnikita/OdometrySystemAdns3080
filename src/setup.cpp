#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/ethernet/mac.h>
#include <libopencm3/ethernet/phy.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>

#include "setup.hpp"

// тактирование
void SetupPeriph::clockSetup() 
{
    // разгоняем мк
    rcc_clock_setup_pll(&rcc_hse_16mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

    // тактируем линии
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_GPIOD);
    rcc_periph_clock_enable(RCC_GPIOE);

    // тактирование DMA
    rcc_periph_clock_enable(RCC_DMA1);
    rcc_periph_clock_enable(RCC_DMA2);

    // тактирование таймера TIM6
    rcc_periph_clock_enable(RCC_TIM6);

    // тактирование SPI
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_SPI3);
    
    // тактирование USART2
    rcc_periph_clock_enable(RCC_USART2);
}

// конфигурация таймера для отсчета временных задержек
void SetupPeriph::timerSetup() 
{
	// конфигурация вывода
    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);
    
    // rcc_apb1_frequency = 42 МГц
	timer_set_prescaler(TIM6, (rcc_apb1_frequency * 2) - 1);  // 84 МГц / 84 = 2 МГц
	timer_set_period(TIM6, 0xFFFF);     // 1 мкс
	timer_enable_counter(TIM6);
}

// задержка в микросекундах
void SetupPeriph::delayUs(uint16_t delay)
{
	// ждем реакции датчика
	uint32_t start = timer_get_counter(TIM6);
    while ((uint16_t)(timer_get_counter(TIM6) - start) < delay) {
		__asm__("NOP");
	}
}

// конфигурация портов MAC
void SetupPeriph::macGpioSetup()
{
    // REF_CLK, MDIO, CRS_DV
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO1 | GPIO2 | GPIO7);
    gpio_set_af(GPIOA, GPIO_AF11, GPIO1 | GPIO2 | GPIO7);

    // TXEN, TXD0 и TXD1
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12 | GPIO13);
    gpio_set_af(GPIOB, GPIO_AF11, GPIO11 | GPIO12 | GPIO13);

    // MDC, RXD0, RXD1
    gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO1 | GPIO4 | GPIO5);
    gpio_set_af(GPIOC, GPIO_AF11, GPIO1 | GPIO4 | GPIO5);
}

// сбрасываем настройки PHY
void SetupPeriph::resetPhy()
{
    // RESET
    gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);
    // сбрасываем состояние phy
    gpio_clear(GPIOE, GPIO15);

    // удерживаем сигнал nRST 400 мкс в состоянии 0
    delayUs(T_nRSTIA);
    // поднимаем сигнал перезагрузки
    gpio_set(GPIOE, GPIO15);

    // ждем 5 мс (5000 мкс) для доступа к PHY по SMI
    delayUs(T_AFTERnRST);
}

// конфигурация MAC контроллера
void SetupPeriph::macSetup()
{
    resetPhy();

    // тактируем контроллер конфигурации системы
    // (используется для выбора Ethernet PHY)
    rcc_periph_clock_enable(RCC_SYSCFG);

    // до тактирования MAC устанавливаем режим RMII
    // для этого выставляем бит 23 MII_RMII_SEL в единицу
    SYSCFG_PMC |= (1 << 23);

    // тактируем модуль MAC
    rcc_periph_clock_enable(RCC_ETHMAC);
    rcc_periph_clock_enable(RCC_ETHMACTX);
    rcc_periph_clock_enable(RCC_ETHMACRX);

    // конфигурируем выводы
    macGpioSetup();
}

// SPI1 -- левая камера
void SetupPeriph::spi1Setup() 
{
    // конфигурируем порты SPI1 на альтернативные функции:
    // NSS = PA4
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO4);

    gpio_set(GPIOA, GPIO4);

    // SCK = PA5, MISO = PA6
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO5 | GPIO6);
    gpio_set_af(GPIOA, GPIO_AF5, GPIO5 | GPIO6);

    // MOSI = PB5
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO5);
    gpio_set_af(GPIOB, GPIO_AF5, GPIO5);

    // устанавливаем параметры вывода для SCK и MOSI
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO5);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO5);

    spi_disable(SPI1);

    // настраиваем SPI1 как мастер
    // т.к. SPI1 тактируется от APB2 (84 МГц), то 
    // скорость SPI1 равна 84/64 = 1,3 МГц
    spi_init_master(SPI1,                              
                    // конфигурация тактового сигнала
                    // скорость
                    SPI_CR1_BAUDRATE_FPCLK_DIV_64,      

                    // полярность CPOL = 0 -- вне процесса передачи данных
                    // тактовый сигнал удерживается в нуле
                    // при этом передний фронт, по которому происходит захват
                    // данных, определяется как скачок 0-1
                    SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
                    
                    // фаза CPHA = 0 -- данные фиксируются по
                    // переднему фронту тактового сигнала
                    SPI_CR1_CPHA_CLK_TRANSITION_2,

                    SPI_CR1_DFF_8BIT,                   // формат кадра данных - 8 бит
                    SPI_CR1_MSBFIRST);                  // первый бит - старший

    // управляем NSS программно
    spi_enable_software_slave_management(SPI1);
    spi_set_nss_high(SPI1);

    // включаем SPI1
    spi_enable(SPI1);
}
// конфигурируем dma для spi1
// mosi - канал 3 поток 3
void SetupPeriph::spi1TxDmaSetup()
{
    // настраиваем DMA2 для SPI1_TX
	dma_stream_reset(DMA2, DMA_STREAM3);

    // выбираем канал
    dma_channel_select(DMA2, DMA_STREAM3, DMA_SxCR_CHSEL_3);

	// указываем адрес регистра данных SPI1
	dma_set_peripheral_address(DMA2, DMA_STREAM3, (uint32_t)&SPI1_DR);

    // устанавливаем режим отправки данных из памяти в периферию
    dma_set_transfer_mode(DMA2, DMA_STREAM3,
				DMA_SxCR_DIR_MEM_TO_PERIPHERAL);

    // устанавливаем размер данных периферии и памяти
	dma_set_peripheral_size(DMA2, DMA_STREAM3, DMA_SxCR_PSIZE_8BIT);
	dma_set_memory_size(DMA2, DMA_STREAM3, DMA_SxCR_MSIZE_8BIT);
    
    // высокий приоритет для канала DMA
	dma_set_priority(DMA2, DMA_STREAM3, DMA_SxCR_PL_VERY_HIGH);
    
    // после каждой передачи адрес в памяти автоматически 
	// увеличивается (чтобы перейти к следующему байту)
	dma_enable_memory_increment_mode(DMA2, DMA_STREAM3); 
  
	// разрешаем прерывание после завершения передачи
	dma_enable_transfer_complete_interrupt(DMA2, DMA_STREAM3);

    // ВКЛЮЧАТЬ КАНАЛ БУДЕМ В ОПРЕДЕЛЕНИИ ЗАДАЧИ

    nvic_set_priority(NVIC_DMA2_STREAM3_IRQ, 5 << 4);
    nvic_enable_irq(NVIC_DMA2_STREAM3_IRQ);

    // включаем DMA-запросы для SPI1 (передача)
    spi_enable_tx_dma(SPI1);
}
// miso - канал 3 поток 2
void SetupPeriph::spi1RxDmaSetup()
{
   // настраиваем DMA2 для SPI1_RX
	dma_stream_reset(DMA2, DMA_STREAM2);

    // выбираем канал
    dma_channel_select(DMA2, DMA_STREAM2, DMA_SxCR_CHSEL_3);

	// указываем адрес регистра данных SPI1
	dma_set_peripheral_address(DMA2, DMA_STREAM2, (uint32_t)&SPI1_DR);

    // устанавливаем режим отправки данных из памяти в периферию
    dma_set_transfer_mode(DMA2, DMA_STREAM2,
				DMA_SxCR_DIR_PERIPHERAL_TO_MEM);

    // устанавливаем размер данных периферии и памяти
	dma_set_peripheral_size(DMA2, DMA_STREAM2, DMA_SxCR_PSIZE_8BIT);
	dma_set_memory_size(DMA2, DMA_STREAM2, DMA_SxCR_MSIZE_8BIT);
    
    // высший приоритет для канала DMA
	dma_set_priority(DMA2, DMA_STREAM2, DMA_SxCR_PL_VERY_HIGH);
    
    // после каждой передачи адрес в памяти автоматически 
	// увеличивается (чтобы перейти к следующему байту)
	dma_enable_memory_increment_mode(DMA2, DMA_STREAM2); 
  
	// разрешаем прерывание после завершения передачи
	dma_enable_transfer_complete_interrupt(DMA2, DMA_STREAM2);

    // ВКЛЮЧАТЬ КАНАЛ БУДЕМ В ОПРЕДЕЛЕНИИ ЗАДАЧИ

    nvic_set_priority(NVIC_DMA2_STREAM2_IRQ, 5 << 4);
    nvic_enable_irq(NVIC_DMA2_STREAM2_IRQ);

    // включаем DMA прием для SPI1
    spi_enable_rx_dma(SPI1);
}

// SPI3 -- правая камера
void SetupPeriph::spi3Setup() 
{
    // конфигурируем порты SPI3 на альтернативные функции:
    // NSS = PD0
    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);

    gpio_set(GPIOD, GPIO0);

    // SCK = PC10, MISO = PC11, MOSI = PC12
    gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO10 | GPIO11 | GPIO12);
    gpio_set_af(GPIOC, GPIO_AF6, GPIO10 | GPIO11 | GPIO12);

    // устанавливаем параметры вывода для SCK и MOSI
    gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO10 | GPIO12);

    spi_disable(SPI3);
    // rcc_periph_reset_pulse(RST_SPI3);

    // настраиваем SPI3 как мастер
    // т.к. SPI3 тактируется от APB1 (42 МГц), то 
    // скорость SPI3 равна 42/32 = 1,3 МГц
    spi_init_master(SPI3, SPI_CR1_BAUDRATE_FPCLK_DIV_32, 
                    SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
                    SPI_CR1_CPHA_CLK_TRANSITION_2, 
                    SPI_CR1_DFF_8BIT, 
                    SPI_CR1_MSBFIRST);

    // управляем NSS программно
    spi_enable_software_slave_management(SPI3);
    spi_set_nss_high(SPI3);

    // включаем SPI3
    spi_enable(SPI3);
}
// конфигурируем dma1 для spi3
// mosi - канал 0 поток 5
void SetupPeriph::spi3TxDmaSetup()
{
    // настраиваем DMA1 для SPI3_TX
	dma_stream_reset(DMA1, DMA_STREAM5);

    // выбираем канал
    dma_channel_select(DMA1, DMA_STREAM5, DMA_SxCR_CHSEL_0);

	// указываем адрес регистра данных SPI3
	dma_set_peripheral_address(DMA1, DMA_STREAM5, (uint32_t)&SPI3_DR);

    // устанавливаем режим отправки данных из памяти в периферию
    dma_set_transfer_mode(DMA1, DMA_STREAM5,
				DMA_SxCR_DIR_MEM_TO_PERIPHERAL);

    // устанавливаем размер данных периферии и памяти
	dma_set_peripheral_size(DMA1, DMA_STREAM5, DMA_SxCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_STREAM5, DMA_SxCR_MSIZE_8BIT);
    
    // высокий приоритет для канала DMA
	dma_set_priority(DMA1, DMA_STREAM5, DMA_SxCR_PL_VERY_HIGH);
    
    // после каждой передачи адрес в памяти автоматически 
	// увеличивается (чтобы перейти к следующему байту)
	dma_enable_memory_increment_mode(DMA1, DMA_STREAM5); 
  
	// разрешаем прерывание после завершения передачи
	dma_enable_transfer_complete_interrupt(DMA1, DMA_STREAM5);

    // ВКЛЮЧАТЬ КАНАЛ БУДЕМ В ОПРЕДЕЛЕНИИ ЗАДАЧИ

    nvic_set_priority(NVIC_DMA1_STREAM5_IRQ, 5 << 4);
    nvic_enable_irq(NVIC_DMA1_STREAM5_IRQ);

    // включаем DMA-запросы для SPI3 (передача)
    spi_enable_tx_dma(SPI3);
}
// miso - канал 0 поток 0
void SetupPeriph::spi3RxDmaSetup()
{
   // настраиваем DMA1 для SPI3_RX
	dma_stream_reset(DMA1, DMA_STREAM0);

    // выбираем канал
    dma_channel_select(DMA1, DMA_STREAM0, DMA_SxCR_CHSEL_0);

	// указываем адрес регистра данных SPI3
	dma_set_peripheral_address(DMA1, DMA_STREAM0, (uint32_t)&SPI3_DR);

    // устанавливаем режим отправки данных из памяти в периферию
    dma_set_transfer_mode(DMA1, DMA_STREAM0,
				DMA_SxCR_DIR_PERIPHERAL_TO_MEM);

    // устанавливаем размер данных периферии и памяти
	dma_set_peripheral_size(DMA1, DMA_STREAM0, DMA_SxCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_STREAM0, DMA_SxCR_MSIZE_8BIT);
    
    // высший приоритет для канала DMA
	dma_set_priority(DMA1, DMA_STREAM0, DMA_SxCR_PL_VERY_HIGH);
    
    // после каждой передачи адрес в памяти автоматически 
	// увеличивается (чтобы перейти к следующему байту)
	dma_enable_memory_increment_mode(DMA1, DMA_STREAM0); 
  
	// разрешаем прерывание после завершения передачи
	dma_enable_transfer_complete_interrupt(DMA1, DMA_STREAM0);

    // ВКЛЮЧАТЬ КАНАЛ БУДЕМ В ОПРЕДЕЛЕНИИ ЗАДАЧИ

    nvic_set_priority(NVIC_DMA1_STREAM0_IRQ, 5 << 4);
    nvic_enable_irq(NVIC_DMA1_STREAM0_IRQ);

    // включаем DMA прием для SPI3
    spi_enable_rx_dma(SPI3);
}

// конфигурируем вспомогательные выводы датчика RST и NPD
void SetupPeriph::adns3080PinsSetup()
{
    // правый датчик 
    // настраиваем вывод RESET и устанавливаем его в ноль
    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
    gpio_clear(GPIOD, GPIO1);
    // настраиваем вывод NPD для нормальной работы датчика
    gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
    gpio_set(GPIOE, GPIO1);
    
    // левый датчик
    // настраиваем вывод RESET и устанавливаем его в ноль
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);
    gpio_clear(GPIOB, GPIO0);
    // настраиваем вывод NPD для нормальной работы датчика
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
    gpio_set(GPIOB, GPIO1);
    
}

// конфигурируем dma1, канал 4, поток 6
void SetupPeriph::usart2DmaSetup()
{
    // настраиваем DMA
	dma_stream_reset(DMA1, DMA_STREAM6);

    // выбираем канал
    dma_channel_select(DMA1, DMA_STREAM6, DMA_SxCR_CHSEL_4);

	// указываем адрес регистра данных USART2, куда DMA будет пересылать данные
	dma_set_peripheral_address(DMA1, DMA_STREAM6, (uint32_t)&USART2_DR);

    // устанавливаем режим отправки данных из памяти в периферию
    dma_set_transfer_mode(DMA1, DMA_STREAM6,
				DMA_SxCR_DIR_MEM_TO_PERIPHERAL);

    // устанавливаем размер данных периферии и памяти
	dma_set_peripheral_size(DMA1, DMA_STREAM6, DMA_SxCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_STREAM6, DMA_SxCR_MSIZE_8BIT);
    
    // высший приоритет для канала DMA
	dma_set_priority(DMA1, DMA_STREAM6, DMA_SxCR_PL_HIGH);
    
    // после каждой передачи адрес в памяти автоматически 
	// увеличивается (чтобы перейти к следующему байту)
	dma_enable_memory_increment_mode(DMA1, DMA_STREAM6); 
  
	// разрешаем прерывание после завершения передачи
	dma_enable_transfer_complete_interrupt(DMA1, DMA_STREAM6);

    // ВКЛЮЧАТЬ КАНАЛ БУДЕМ В ОПРЕДЕЛЕНИИ ЗАДАЧИ

    nvic_set_priority(NVIC_DMA1_STREAM6_IRQ, 5 << 4);
    nvic_enable_irq(NVIC_DMA1_STREAM6_IRQ);

    // включаем usart2_tx
    usart_enable_tx_dma(USART2);
	usart_enable(USART2);
}

// конфигурация USART2
void SetupPeriph::usart2Setup() 
{
    // настраиваем вывод USART2_TX
    gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO5);
    gpio_set_af(GPIOD, GPIO_AF7, GPIO5);

    gpio_set_output_options(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO5);

    // настраиваем UART
    usart_set_baudrate(USART2, BAUD_SPEED);
    usart_set_databits(USART2, WORD_SIZE);
    usart_set_stopbits(USART2, USART_STOPBITS_1);
    usart_set_mode(USART2, USART_MODE_TX);
    usart_set_parity(USART2, USART_PARITY_NONE);
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

    // включаем UART
    usart_enable(USART2);
}
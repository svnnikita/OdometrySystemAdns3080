#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>

#include <cstdio>	

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "adns3080.hpp"

// конструктор
ADNS3080::ADNS3080(const Adns3080Pins &pins) 
	: _cs_gpio_port(pins.cs_gpio_port), 
	  _cs_gpio_pin(pins.cs_gpio_pin),
	  _spi(pins.spi),
	  _reset_gpio_port(pins.reset_gpio_port),
	  _reset_gpio_pin(pins.reset_gpio_pin),
	  _dma(pins.dma),
	  _stream_tx(pins.stream_tx),
	  _stream_rx(pins.stream_rx)
{

}

// сконфигурируем датчик
bool ADNS3080::setup(const bool led_mode, const bool resolution) 
{
	// перезапускаем датчик
    reset();
    delayUs(ADNS3080_T_IN_RST);

	// конфигурируем датчик:
	//                          LED Shutter     High resolution
	uint8_t mask = 0b00000000 | led_mode << 6 | resolution << 4;

	// записываем конфигурацию в датчик
    writeRegister(ADNS3080_CONFIGURATION_BITS, mask);
	delayUs(ADNS3080_T_SWW);

	// читаем регистр конфигурации
	uint8_t config = readRegister(ADNS3080_CONFIGURATION_BITS);
	delayUs(ADNS3080_T_SWW);

	uint8_t result = readRegister(ADNS3080_PRODUCT_ID);
	delayUs(ADNS3080_T_SWW);

	if (config == mask && result == ADNS3080_PRODUCT_ID_VALUE)
		return true;
	else 
		return false;
}

void ADNS3080::loadSROM(const uint8_t *data, uint16_t length) {
    // Шаг 1: аппаратный сброс
    reset();

    // Шаг 2-4: инициализация перед загрузкой
    writeRegister(0x20, 0x44);
    writeRegister(0x24, 0x07);
    writeRegister(0x24, 0x88);

    // Шаг 5: минимум 1 кадровый период (при 2000 fps ~ 500 мкс, берём 1 мс)
    delayUs(1000);

    // Шаг 6: включение режима загрузки SROM
    writeRegister(0x14, 0x18);  // SROM_Enable

    // Шаг 7: burst-запись массива в регистр SROM_Load (0x60)
    csLow();

    // Отправляем адрес регистра SROM_Load с битом записи (0x60 | 0x80 = 0xE0)
    spi_send(_spi, 0x60 | 0x80);
    while (!(SPI_SR(_spi) & SPI_SR_TXE));

    // Отправляем первый байт
    spi_send(_spi, data[0]);
    while (!(SPI_SR(_spi) & SPI_SR_TXE));
    delayUs(ADNS3080_T_LOAD);  // 10 мкс

    // Отправляем остальные байты с задержкой t_LOAD между ними
    for (uint16_t i = 1; i < length; i++) {
        spi_send(_spi, data[i]);
        while (!(SPI_SR(_spi) & SPI_SR_TXE));
        delayUs(ADNS3080_T_LOAD);
    }

    // Дожидаемся завершения передачи на шине
    while (SPI_SR(_spi) & SPI_SR_BSY);

    // Шаг 8: выход из burst-режима, CS поднять на t_BEXIT
    csHigh();
    delayUs(ADNS3080_T_BEXIT);  // 4 мкс
}

// задержка в микросекундах
void ADNS3080::delayUs(uint16_t delay) 
{
	// ждем реакции датчика
	uint32_t start = timer_get_counter(TIM6);
    while ((uint16_t)(timer_get_counter(TIM6) - start) < delay) {
		__asm__("NOP");
	}
}

// читаем регистры
uint8_t ADNS3080::readRegister(const uint8_t reg)
{
	// переменная для значения из указанного регистра
	uint8_t data;

    csLow();

	// отправляем адрес регистра
	spi_xfer(_spi, reg);

	if (reg == ADNS3080_MOTION || reg == ADNS3080_MOTION_BURST)
        delayUs(ADNS3080_T_SRAD_MOT);
    else
		// для остальных регистров задержка 50 мкс
        delayUs(ADNS3080_T_SRAD);

	data = spi_xfer(_spi, 0x00);

	csHigh();

	delayUs(ADNS3080_T_SWW);

    return data;
}

// записываем значения в регистры
void ADNS3080::writeRegister(const uint8_t reg, uint8_t output) 
{
  	csLow();

	// устанавливаем бит 7 регистра в единицу для записи
    spi_send(_spi, reg | 0x80);          
    // ждём окончания отправки адреса
    while (!(SPI_SR(_spi) & SPI_SR_TXE));

	// отправляем данные
    spi_send(_spi, output);              
    // ждём окончания отправки данных
    while (!(SPI_SR(_spi) & SPI_SR_TXE));
    // ждём освобождения шины
    while (SPI_SR(_spi) & SPI_SR_BSY);

    csHigh();

	while (SPI_SR(_spi) & SPI_SR_RXNE) {
        (void)SPI_DR(_spi);
    }

    delayUs(ADNS3080_T_SWW);           
}

// отправляет команду датчику на отправку данных о
// качестве поверхности, движении и т.д.
void ADNS3080::motionBurst(MotionData &data)
{
	// опускаем линию
	csLow();

	// отправляем адрес регистра ADNS3080_MOTION_BURST
	spi_xfer(_spi, ADNS3080_MOTION_BURST);

	// ждем 75 мкс
	delayUs(ADNS3080_T_SRAD_MOT);

	// читаем 7 байт последовательно
	uint8_t raw_motion = spi_xfer(_spi, 0x00);
	data.motion = (raw_motion & 0x80) ? 1 : 0;	// 1 -- движение было, 0 -- движения не было

	// смещения
	data.dx = (uint8_t)spi_xfer(_spi, 0x00);
	data.dy = (uint8_t)spi_xfer(_spi, 0x00);

	// качество поверхности
	data.squal = spi_xfer(_spi, 0x00);	
	// значения затвора -- старший и младший биты
	uint8_t shutter_upper = spi_xfer(_spi, 0x00);
	uint8_t shutter_lower = spi_xfer(_spi, 0x00);
	data.shutter = (uint16_t)((shutter_upper << 8) | shutter_lower);

	// максимальное значение пикселя в кадре
	data.max_pix = spi_xfer(_spi, 0x00);

	csHigh();

	delayUs(ADNS3080_T_SWW);
}

// читаем полные данные о перемещении при помощи dma
void ADNS3080::
	motionBurstDma(MotionData &data, 
				   SemaphoreHandle_t &_dma_tx_semaphore, 
				   SemaphoreHandle_t &_dma_rx_semaphore)
{
	// создаем буфферы для получения и отправки данных
	uint8_t tx_cmd_buf[1] = {ADNS3080_MOTION_BURST};
	uint8_t tx_dummy_buf[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t rx_buf[7];

	csLow();

	// передаем датчику адрес регистра
	dma_disable_stream(_dma, _stream_tx);
	dma_set_memory_address(_dma, _stream_tx, (uint32_t)tx_cmd_buf);
    dma_set_number_of_data(_dma, _stream_tx, 1);

	// очищаем флаги прерываний dma
	dma_clear_interrupt_flags(_dma, _stream_tx, DMA_TCIF);
	dma_enable_stream(_dma, _stream_tx);

	// ждем завершения передачи адреса
	xSemaphoreTake(_dma_tx_semaphore, portMAX_DELAY);

	// дожидаемся физического окончания передачи байта по шине
	while (SPI_SR(_spi) & SPI_SR_BSY);

	// очищаем приемник от мусора
	volatile uint8_t dummy = SPI_DR(_spi);
	(void)dummy;

	delayUs(ADNS3080_T_SWW);

	// запускаем передачу и прием
	dma_disable_stream(_dma, _stream_tx); // TX
    dma_disable_stream(_dma, _stream_rx); // RX

	// очищаем флаги прерываний каналов передачи и приема
	dma_clear_interrupt_flags(_dma, _stream_tx, DMA_TCIF);
	dma_clear_interrupt_flags(_dma, _stream_rx, DMA_TCIF);

	// настраиваем адрес памяти, куда будем принимать данные
    dma_set_memory_address(_dma, _stream_rx, (uint32_t)rx_buf);
    dma_set_number_of_data(_dma, _stream_rx, 7);

	// настраиваем адрес памяти, откуда будем брать фиктивные данные
    dma_set_memory_address(_dma, _stream_tx, (uint32_t)tx_dummy_buf);
    dma_set_number_of_data(_dma, _stream_tx, 7);

	// включаем оба потока
	dma_enable_stream(_dma, _stream_rx);
    dma_enable_stream(_dma, _stream_tx);

	// ждем завершения приема
	xSemaphoreTake(_dma_rx_semaphore, portMAX_DELAY);

	csHigh();

	// копируем данные
    data.motion = (rx_buf[0] & 0x80) ? 1 : 0;
    data.dx = (int8_t)rx_buf[1];
    data.dy = (int8_t)rx_buf[2];
    data.squal = rx_buf[3];
    data.shutter = (int16_t)((rx_buf[4] << 8) | rx_buf[5]);
    data.max_pix = rx_buf[6];
}


// запускает передачу данных о перемещении
// для этого необходимо прочитать данные регистра ADNS3080_MOTION_BURST
void ADNS3080::displacement(uint8_t *dx, uint8_t *dy)
{
	// опускаем линию
	csLow();
	

}

// восстанавливаем пиксели следующего кадра:
void ADNS3080::frameCapture(uint8_t output[ADNS3080_PIXELS][ADNS3080_PIXELS]) 
{  
	// первый пиксель:
	uint8_t pixel = 0;

	// отправляем значение в регистр
	writeRegister(ADNS3080_FRAME_CAPTURE, 0x83);
	
	// опускаем линию и начинаем получение данных
	csLow();

	// получаем пиксели до тех пор, пока не будет найден первый
	while((pixel & 0b01000000) == 0) {
		
		// отправляем любой бит для получения данных из указанного регистра 
		spi_send(_spi, 0x00);	
		while (!(SPI_SR(_spi) & SPI_SR_RXNE));

		// получаем пиксель
		pixel = spi_read(_spi);
		
		delayUs(ADNS3080_T_LOAD);  
	}
	
	// анализируем первый кадр:
	for(int y = 0; y < ADNS3080_PIXELS; y++) {
		for(int x = 0; x < ADNS3080_PIXELS; x++) {  
		
			// сохраняем и масштабируем полученный кадр
			output[x][y] = pixel << 2; 

			// получаем следующий пиксель
			spi_send(_spi, 0x00);
			while (!(SPI_SR(_spi) & SPI_SR_RXNE));

			pixel = spi_read(_spi);
			delayUs(ADNS3080_T_LOAD);  
		}
	}

	// отключаем линию
	csHigh();

	// ждем реакцию датчика
	delayUs(ADNS3080_T_LOAD + ADNS3080_T_BEXIT);
}   	
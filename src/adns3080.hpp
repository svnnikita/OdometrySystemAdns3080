#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#ifndef ADNS3080_h
#define ADNS3080_h 

constexpr uint16_t ADNS3080_T_IN_RST = 500;	// время задержки после сброса (мкс)
constexpr uint8_t ADNS3080_T_PW_RESET = 10; // длительность импульса сброса (мкс)

// Задержка между отправкой адреса регистра и началом чтения данных (мкс)
// Константа применяется для регистров движения (Motion registers)
constexpr uint8_t ADNS3080_T_SRAD_MOT = 75;   

// Интервал времени между командами записи (мкс)
constexpr uint8_t ADNS3080_T_SWW = 50;

// Задержка между отправкой адреса регистра и началом чтения данных (мкс)
// Константа применяется для всех регистров, кроме регистров движения (см. выше)
constexpr uint8_t ADNS3080_T_SRAD = 50;

// Задержка побайтного захвата кадров при загрузке и повторной загрузке SROM (мкс)
constexpr uint8_t ADNS3080_T_LOAD = 10;

// Время, в течение которого необходимо выдержать высокий уровень сигнала 
// при выходе из режима frame capture
constexpr uint8_t ADNS3080_T_BEXIT = 4;

// разрешение датчика
constexpr uint8_t ADNS3080_PIXELS = 30;

class ADNS3080 
{ 
public:
	// перечисление с адресами регистров
	enum Register : uint8_t {
		ADNS3080_PRODUCT_ID = 0x00,			// для проверки соединения
		ADNS3080_MOTION = 0x02,				// чтение, для проверки движения
		ADNS3080_CONFIGURATION_BITS = 0x0A, // для конфигурации датчика
		ADNS3080_MOTION_CLEAR = 0x12, 		// для обнуления счетчиков движения	
		ADNS3080_FRAME_CAPTURE = 0x13,		// записываем в регистр 0х83 и получаем изображение
		ADNS3080_PRODUCT_ID_VALUE = 0x17,	// содержит уникальный идентификатор, 
											// присвоенный ADNS-3080
		ADNS3080_PIXEL_BURST = 0x40,		// для доступа ко всем значениям пикселей
		ADNS3080_MOTION_BURST = 0x50,		// пакетный режим, для доступа к 
											// регистрам Motion, Delta_X и 
											// Delta_Y, SQUAL, Shutter_ Upper, 
											// Shutter_Lower и Maximum_Pixel
	};

	// структура с наименованием выводов датчика
	struct Adns3080Pins 
	{
		uint32_t cs_gpio_port;		// группа выводов, на которой расположен используемый SPI
		uint16_t cs_gpio_pin;		// вывод, на котором расположен Chip Select
		uint32_t spi;				// номер используемого SPI
		uint32_t reset_gpio_port;	// группа выводов, на которой расположен вывод reset
		uint16_t reset_gpio_pin;	// вывод, на котором расположен reset
	};

	// структура со значениями перемещения, качества повехности и т.д.
	// используется в методе motionBurst()
	struct MotionData
	{
		uint8_t motion;		// 1 -- движение было, 0 -- движения не было
		int8_t dx;			// смещение по X
		int8_t dy;			// смещение по Y
        uint8_t squal;		// качество поверхности
		uint16_t shutter;	// значение затвора (в тактах)
		uint8_t max_pix;	// максимальное значение пикселя в кадре
	};

	// передадим в конструктор параметры для конфигурации периферии
	ADNS3080(const Adns3080Pins &pins);	

	// инициализация датчика
	bool setup(const bool led_mode = true, const bool resolution = true);

	// загружаем SROM
	void loadSROM(const uint8_t *srom_data, uint16_t length);

	// функция задержки для корректной работы датчика
	void delayUs(uint16_t delay);

	// чтение регистров датчика
	uint8_t readRegister(const uint8_t);

	// запись в регистры датчика
	void writeRegister(const uint8_t, uint8_t);

	// перезагружаем датчик
	void reset() 
	{
		// подаем на вывод перезагрузки высокий сигнал
		gpio_set(_reset_gpio_port, _reset_gpio_pin);
		// ждем реакции датчика
		delayUs(ADNS3080_T_PW_RESET);
		// опускаем сигнал
		gpio_clear(_reset_gpio_port, _reset_gpio_pin);
		// ждем реакции датчика
		delayUs(ADNS3080_T_IN_RST);      
	}

	// очищаем регистры смещения, DELTA_X и DELTA_Y
	void motionClear()
	{
		writeRegister(ADNS3080_MOTION_CLEAR, 0x00);
	} 

	// читаем полные данные о перемещении
	// для этого необходимо отправить адрес регистра ADNS3080_MOTION_BURST
	void motionBurst(MotionData &data);

	void motionBurstDma(MotionData &data, 
				        SemaphoreHandle_t &_dma_tx_semaphore, 
				        SemaphoreHandle_t &_dma_rx_semaphore);

	// запуск передачи данных только о перемещении
	void displacement(uint8_t *dx, uint8_t *dy);

	// запуск передачи изображения с датчика
	void frameCapture(uint8_t[ADNS3080_PIXELS][ADNS3080_PIXELS]);

private:
	// выводы датчика
	uint32_t _cs_gpio_port;		// группа выводов, на которой расположен используемый SPI
	uint16_t _cs_gpio_pin;		// вывод, на котором расположен Chip Select
	uint32_t _spi;				// номер используемого SPI
	uint32_t _reset_gpio_port;	// группа выводов, на котором расположен вывод reset
	uint16_t _reset_gpio_pin;	// вывод, на котором расположен reset

	// опускаем chip select для связи с датчиком
	void csLow() { gpio_clear(_cs_gpio_port, _cs_gpio_pin); }
	// поднимаем chip select для завершения связи
	void csHigh() { gpio_set(_cs_gpio_port, _cs_gpio_pin); }
};

#endif
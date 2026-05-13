// lan8720_config.cpp -- файл с описанием функций драйвера

#include <libopencm3/stm32/usart.h>
#include <libopencm3/ethernet/mac.h>
#include <libopencm3/ethernet/phy.h>

#include <cstdio>

#include "lan8720_config.hpp"

// задержка в микросекундах
void Lan8720Config::delayUs(uint16_t delay) 
{
    // ждем реакции датчика
    uint32_t start = timer_get_counter(TIM6);
    while ((uint16_t)(timer_get_counter(TIM6) - start) < delay) {
        __asm__("NOP");
    }
}

// узнаем уникальный серийный номер мк
void Lan8720Config::readUid()
{
    uint16_t *idBase0 = (uint16_t*)UID_BASE;
    uint16_t *idBase1 = (uint16_t*)(UID_BASE + 0x02);
    uint32_t *idBase2 = (uint32_t*)(UID_BASE + 0x04);
    uint32_t *idBase3 = (uint32_t*)(UID_BASE + 0x08);

    char buffer[64];
    uint32_t len = sprintf(buffer, "UID %04x-%04x-%08lx-%08lx\n", 
                    *idBase0, *idBase1, (unsigned long)*idBase2, (unsigned long)*idBase3);
    // Отправка через USART1 (предварительно должен быть инициализирован)
    for (uint32_t i = 0; i < len; i++) {
        usart_send_blocking(USART2, buffer[i]);
    }
}

// прочитаем id1/id2 lan8720
uint16_t Lan8720Config::lan8720ReadPhyId1(uint8_t phy_addr)
{
    return eth_smi_read(phy_addr, LAN8720_REG_PHY_IDENTIFIER_1);
    // должна возвращать 0x0007
}
uint16_t Lan8720Config::lan8720ReadPhyId2(uint8_t phy_addr)
{
    return eth_smi_read(phy_addr, LAN8720_REG_PHY_IDENTIFIER_2);
    // должна возвращать 0xC0F1
}

// сброс программного обеспечения lan8720
void Lan8720Config::lan8720Reset(uint8_t phy_addr)
{
    // устанавливаем бит 15 Soft Reset в 1
    // для этого мы маску из 16-ти бит 0x8000 регистр
    eth_smi_write(phy_addr, LAN8720_REG_BASIC_CONTROL, 0x8000);
    // читаем значение регистра и убеждаемся, что перезагрузка прошла успешно
    while (eth_smi_read(phy_addr, LAN8720_REG_BASIC_CONTROL) & 0x8000);
    // включаем автоматическое согласование (Auto-Negotiation)
    // для этого устанавливаем бит 12 в единицу
    eth_smi_bit_set(phy_addr, LAN8720_REG_BASIC_CONTROL, 0x1000);
}

// проверяем, установлен ли линк
uint8_t Lan8720Config::lan8720WaitLink(uint8_t phy_addr, uint32_t timeout)
{
    // читаем регистр указанное количество раз
    while (timeout--) {
        // читаем значение регистра
        uint16_t bsr = eth_smi_read(phy_addr, LAN8720_REG_BASIC_STATUS);
        // проверяем значение
        if (bsr & 0x0004) return 1;
        // ожидаем некоторое время
        delayUs(1000);
    }
    return 0;
}
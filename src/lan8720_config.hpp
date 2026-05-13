// lan8720_config.hpp -- заголовочный файл для драйвера 
// микросхемы lan8720

#include <libopencm3/stm32/timer.h>

// регистр, содержащий уникальный id мк
constexpr uint32_t UID_BASE = 0x1FFF7A10;

class Lan8720Config {
public:
    // конструктор
    Lan8720Config() {}

    // регистры lan8720
    enum Lan8720Registers : uint16_t {
        LAN8720_REG_BASIC_CONTROL = 0x00,
        LAN8720_REG_BASIC_STATUS = 0x01,
        LAN8720_REG_PHY_IDENTIFIER_1 = 0x02,
        LAN8720_REG_PHY_IDENTIFIER_2 = 0x03,
    };

    // задержка в микросекундах
    void delayUs(uint16_t delay);

    // узнаем уникальный серийный номер мк
    void readUid();

    // прочитаем id1/id2 lan8720
    uint16_t lan8720ReadPhyId1(uint8_t phy_addr);
    uint16_t lan8720ReadPhyId2(uint8_t phy_addr);

    // программный сброс lan8720
    void lan8720Reset(uint8_t phy_addr);

    // проверяем, установлен ли линк
    uint8_t lan8720WaitLink(uint8_t phy_addr, uint32_t timeout);

    void lan8720AutoNegEnable(uint8_t phy_addr)
    {
        // BCR bit12 = Auto-Negotiation Enable
        eth_smi_bit_set(phy_addr, LAN8720_REG_BASIC_CONTROL, 0x1000);
    }

private:
    // MAC-адрес
    const uint8_t mac[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
};
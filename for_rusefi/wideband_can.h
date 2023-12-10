#pragma once

#define RUSEFI_WIDEBAND_VERSION (0xA0)

// ascii "rus"
#define WB_ACK 0x727573

#define WB_BL_HEADER 0x0EF
#define WB_OPCODE_START 0
#define WB_OPCODE_ERASE 1
#define WB_ERASE_TAG 0x5A5A
#define WB_OPCODE_DATA 2
#define WB_OPCODE_REBOOT 3

#define WB_BL_BASE (WB_BL_HEADER << 4)

// 0xEF0'0000
#define WB_BL_ENTER ((WB_BL_BASE + WB_OPCODE_START) << 16)
// 0xEF1'5A5A
#define WB_BL_ERASE ((WB_BL_BASE + WB_OPCODE_ERASE) << 16 + WB_ERASE_TAG)
// 0xEF2'0000
#define WB_BL_DATA_BASE ((WB_BL_BASE + WB_OPCODE_DATA) << 16)
// 0xEF3'0000
#define WB_BL_REBOOT ((WB_BL_BASE + WB_OPCODE_REBOOT) << 16)
#define WB_MSG_SET_INDEX 0xEF4'0000
#define WB_MGS_ECU_STATUS 0xEF5'0000
#define WB_DATA_BASE_ADDR 0x190

// we transmit every 10ms
#define WBO_TX_PERIOD_MS 10

namespace wbo
{
enum class Fault : uint8_t
{
    None = 0,

    // First fault code at 3 so it's easier to see blink code
    SensorDidntHeat = 3,
    SensorOverheat = 4,
    SensorUnderheat = 5,
    SensorNoHeatSupply = 6,
};

struct StandardData
{
    // DO NOT move the version field - its position and format must be
    // fixed so that incompatible versions can be identified.
    uint8_t Version;
    uint8_t Valid;

    uint16_t Lambda;
    uint16_t TemperatureC;

    uint16_t pad;
};

struct DiagData
{
    uint16_t Esr;
    uint16_t NernstDc;
    uint8_t PumpDuty;
    Fault Status;

    uint8_t HeaterDuty;
    uint8_t pad;
};

static inline const char* describeFault(Fault fault) {
    switch (fault) {
        case Fault::None:
            return "OK";
        case Fault::SensorDidntHeat:
            return "Sensor failed to heat";
        case Fault::SensorOverheat:
            return "Sensor overheat";
        case Fault::SensorUnderheat:
            return "Sensor underheat";
        case Fault::SensorNoHeatSupply:
            return "Sensor no heat supply";
    }

    return "Unknown";
}

// AEMNet protocol

#define AEMNET_UEGO_TX_PERIOD_MS    10
#define AEMNET_UEGO_BASE_ID         0x00000180

// 29 bit ID, 500kbs, rate 100 hz, endian big, DLC 8
// ID: 0x180 .. 0x18f
struct AemNetUEGOData
{
    // 0.0001 Lambda/bit, 0 to 6.5535 Lambda
    uint16_t Lambda;
    // 0.001 %/bit, -32.768% to 32.768%
    uint16_t Oxygen;
    // 0.1 V/bit, 0 to 25.5 Volts
    uint8_t SystemVolts;
    uint8_t reserved;
    // [1] - Bosch LSU4.9 detected
    // [5] - Free-Air cal in use
    // [7] - Lambda data valid
    uint8_t Flags;
    // [6] - Sensor Fault
    uint8_t Faults;
};

#define AEMNET_EGT_TX_PERIOD        50
#define AEMNET_EGT_BASE_ID          0x000A0305

// 29 bit ID, 500kbs, rate 20 hz, endian big, DLC 8
// ID: 0x000A0305
struct AemNetEgtData
{
    // 1 degC/bit, 0 to 65535 degC
    uint16_t TemperatureC;
    uint8_t pad[6];
};

} // namespace wbo

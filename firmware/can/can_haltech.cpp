#include "can.h"
#include "hal.h"

#include "can_haltech.h"

#include "util/byteswap.h"
#include "port.h"
#include "status.h"
#include "can_helper.h"
#include "sampling.h"
#include "pump_dac.h"
#include "heater_control.h"
#include "lambda_conversion.h"
#include "max3185x.h"
#include "../for_rusefi/wideband_can.h"

// Haltech protocol
// 1Mbps, big endian, DLC 8

#define HALTECH_WB2_BASE_ID         0x2B0

namespace haltech
{

enum class SensorFlags : uint8_t {
    None = 0,
    LowBattery = 1,
    HighBattery = 2,
    SensorShortCircuit = 3,
    SensorOpenCircuit = 4,
    SensorCold = 5,
};

// ID: WB2A 0x2B0
// ID: WB2B 0x2B4
// ID: WB2C 0x2B6
// ID: WB2D 0x2B8
struct AfrData1
{
    beuint16_t Lambda1;        // 32767 = Free Air, otherwise y = x/1024
    beuint16_t Lambda2;        // 0.001 Lambda/bit, 0.000 to 65.535 Lambda
    uint8_t RSense1;           // 0-255 Ohm; 1/1
    uint8_t RSense2;           // 0-255 Ohm; 1/1
    SensorFlags Sensor1Flags : 4;
    SensorFlags Sensor2Flags : 4;
    uint8_t VBatt;             // y = x * 20 / 255 Volts
} __attribute__((packed));

static_assert(sizeof(AfrData1) == 8);

} //namespace haltech


void SendHaltechAfrFormat(Configuration* configuration, uint8_t ch)
{
    if (ch % 2 != 0)
        return; // Haltech protocol sends both channels in one message

    auto id = HALTECH_WB2_BASE_ID; // WB2A
    switch (configuration->afr[ch].ExtraCanIdOffset) {
        case 1:
            id += 4; break; // WB2B
        case 2:
            id += 6; break; // WB2C
        case 3:
            id += 8; break; // WB2D
    }

    CanTxTyped<haltech::AfrData1> frame(id, true);

    const auto& sampler1 = GetSampler(ch);

    frame.get().VBatt = sampler1.GetInternalHeaterVoltage() * 255.0 / 20.0f;

    float lambda = GetLambda(ch);
    frame.get().Lambda1 = LambdaIsValid(ch, lambda) ? lambda * 1024 : 0;
    frame.get().RSense1 = sampler1.GetSensorInternalResistance();

    frame.get().Sensor1Flags = haltech::SensorFlags::None;
    
    // If next channel is not turned off, read it as well for dual sensor setups
    if (ch + 1 < AFR_CHANNELS && configuration->afr[ch+1].ExtraCanProtocol == CanProtocol::Haltech) {
        const auto& sampler2 = GetSampler(ch+1);
        float lambda2 = GetLambda(ch + 1);
        frame.get().Lambda2 = LambdaIsValid(ch + 1, lambda2) ? lambda2 * 1024 : 0;
        frame.get().RSense2 = sampler2.GetSensorInternalResistance();

        frame.get().Sensor2Flags = haltech::SensorFlags::None;
    }
}

#if (EGT_CHANNELS > 0)

#define HALTECH_TCA_BASE_ID         0x2CC

namespace haltech
{
    
struct EgtData
{
    beint16_t Egt[4];
} __attribute__((packed));

static_assert(sizeof(EgtData) == 8);

} //namespace haltech

// id 716 for "box a assigned to can tc 1-4" and
// id 717 for "box b assigned to can tc 5-8"
// Multiplier of 2381 divider of 5850 and offset of -250.

void SendHaltechEgtFormat(Configuration* configuration, uint8_t ch)
{
    if (ch != 0)
        return; // Haltech protocol sends 1-4 channels in one message

    auto id = HALTECH_TCA_BASE_ID + configuration->egt[ch].ExtraCanIdOffset;

    CanTxTyped<haltech::EgtData> frame(id, true);

    for (uint8_t i = 0; i < EGT_CHANNELS; i++)
    {
        frame.get().Egt[i] = (getEgtDrivers()[i].temperature + 250.0f) * 5850.0f / 2381.0f;
    }
}

#endif

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
    // If channel 0 is configured for Haltech, channel 1 data is sent in the same message
    if (ch == 1 && configuration->afr[0].ExtraCanProtocol == CanAfrProtocol::Haltech)
        return;

    uint8_t channel0enabled = configuration->afr[0].ExtraCanProtocol == CanAfrProtocol::Haltech;

    #if AFR_CHANNELS > 1
    uint8_t channel1enabled = configuration->afr[1].ExtraCanProtocol == CanAfrProtocol::Haltech;
    #endif

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

    float vbatt = 0;

    if (channel0enabled) {
        const auto& sampler0 = GetSampler(0);
        float lambda1 = GetLambda(0);
        frame.get().Lambda1 = LambdaIsValid(0, lambda1) ? lambda1 * 1024 : 0;
        frame.get().RSense1 = sampler0.GetSensorInternalResistance();

        frame.get().Sensor1Flags = haltech::SensorFlags::None;

        vbatt = sampler0.GetInternalHeaterVoltage();
    }
    
    #if (AFR_CHANNELS > 1)

    if (channel1enabled) {
        const auto& sampler2 = GetSampler(1);
        float lambda2 = GetLambda(1);
        frame.get().Lambda2 = LambdaIsValid(1, lambda2) ? lambda2 * 1024 : 0;
        frame.get().RSense2 = sampler2.GetSensorInternalResistance();

        frame.get().Sensor2Flags = haltech::SensorFlags::None;
        float vbatt2 = sampler2.GetInternalHeaterVoltage();

        if (vbatt2 > vbatt) {
            vbatt = vbatt2;
        }
    }

    #endif
    
    frame.get().VBatt = vbatt * 255.0 / 20.0f;
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

void SendHaltechEgtFormat(Configuration* configuration)
{
    auto id = HALTECH_TCA_BASE_ID + configuration->egt[0].ExtraCanIdOffset;

    CanTxTyped<haltech::EgtData> frame(id, true);

    for (uint8_t i = 0; i < EGT_CHANNELS; i++)
    {
        if (!configuration->egt[i].ExtraCanChannelEnabled)
            continue;

        frame.get().Egt[i] = (getEgtDrivers()[i].temperature + 250.0f) * 5850.0f / 2381.0f;
    }
}

#endif

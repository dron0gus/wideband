#include "can.h"
#include "hal.h"

#include "util/byteswap.h"

#include "can_helper.h"
#include "can_aemnet.h"

#include "port.h"
#include "status.h"
#include "heater_control.h"
#include "lambda_conversion.h"
#include "sampling.h"
#include "pump_dac.h"
#include "max3185x.h"

// AEMNet protocol

#define AEMNET_UEGO_TX_PERIOD_MS    10
#define AEMNET_UEGO_BASE_ID         0x00000180

namespace aemnet
{
// 29 bit ID, 500kbs, rate 100 hz, endian big, DLC 8
// ID: 0x180 .. 0x18f
struct UEGOData
{
    // 0.0001 Lambda/bit, 0 to 6.5535 Lambda
    beuint16_t Lambda;
    // 0.001 %/bit, -32.768% to 32.768%
    beuint16_t Oxygen;
    // 0.1 V/bit, 0 to 25.5 Volts
    uint8_t SystemVolts;
    uint8_t reserved;
    // [1] - Bosch LSU4.9 detected
    // [5] - Free-Air cal in use
    // [7] - Lambda data valid
    uint8_t Flags;
    // [6] - Sensor Fault
    uint8_t Faults;
} __attribute__((packed));

static_assert(sizeof(UEGOData) == 8);

} //namespace aemnet

void SendAemNetUEGOFormat(Configuration* cfg, uint8_t ch)
{
    auto id = AEMNET_UEGO_BASE_ID + cfg->afr[ch].ExtraCanIdOffset;
    const auto& sampler = GetSampler(ch);

    CanTxTyped<aemnet::UEGOData> frame(id, true);
    float lambda = GetLambda(ch);
    uint8_t lambdaValid = LambdaIsValid(ch, lambda);

    frame.get().Lambda = lambdaValid ? lambda * 10000 : 0;
    
    frame.get().Oxygen = GetOxygenConcentration(lambda) * 1000;
    frame.get().SystemVolts = sampler.GetInternalHeaterVoltage() * 10;
    frame.get().Flags =
        ((cfg->sensorType == SensorType::LSU49) ? 0x02 : 0x00) |
        ((lambdaValid) ? 0x80 : 0x00);
    frame.get().Faults = 0; //TODO:
}

#if (EGT_CHANNELS > 0)

namespace aemnet
{

#define AEMNET_EGT_TX_PERIOD        50

// AEM EGT Gauge 1800F (30-0305)
#define AEMNET_EGT_0305_BASE_ID          0x000A0305 

struct Egt0305Data
{
    // 1 degC/bit, 0 to 65535 degC
    beuint16_t TemperatureC;
    uint8_t Reserved[6];
} __attribute__((packed));

static_assert(sizeof(Egt0305Data) == 8);

// 8-Channel K-Type CAN Module (30-2224), which supports up to 8 EGT channels
// https://documents.aemelectronics.com/techlibrary_30-2224-_8-channel-k-type-can-module-instructions.pdf
// 0xBA00 and 0xBB00 if using 29 bit IDs 
// 0x5A0 and 0x5B0 if using 11 bit IDs
#define AEMNET_EGT_2224_1_BASE_ID          0x0000BA00
#define AEMNET_EGT_2224_2_BASE_ID          0x0000BB00

// 29 bit ID, 500kbs, rate 20 hz, endian big, DLC 8
// UNIT1 ID: 0x0000BA00 .. 0x0000BA01
// UNIT2 ID: 0x0000BB00 .. 0x0000BB01
struct Egt2224Data
{
    beint16_t Egt[4]; // 0.1 C/bit, -3276.8 to 3276.7 C
} __attribute__((packed));

static_assert(sizeof(Egt2224Data) == 8);

// UNIT1 ID: 0x0000BA02
// UNIT2 ID: 0x0000BB02
struct Egt2224Status
{
    beuint16_t ColdJunctionTemp; // 0.1 C/bit, -3276.8 to 3276.7 C
    uint8_t BatteryVoltage;      // 0 - 25.5 V
    uint8_t Reserved[5];
} __attribute__((packed));

static_assert(sizeof(Egt2224Status) == 8);

} //namespace aemnet


void SendAemNetEGT0305Format(Configuration* cfg)
{
    for (uint8_t ch = 0; ch < EGT_CHANNELS; ch++)
    {
        if (!cfg->egt[ch].ExtraCanChannelEnabled)
            return;

        auto id = AEMNET_EGT_0305_BASE_ID + cfg->egt[ch].ExtraCanIdOffset;
        CanTxTyped<aemnet::Egt0305Data> frame(id, true);
        frame.get().TemperatureC = getEgtDrivers()[ch].temperature;
    }
}

void SendAemNetEGT2224Format(Configuration* cfg)
{
    uint32_t id;
    switch (cfg->egt[0].ExtraCanIdOffset)
    {
        case 0:
            id = AEMNET_EGT_2224_1_BASE_ID;
            break;
        case 1:
            id = AEMNET_EGT_2224_2_BASE_ID;
            break;
        
        default:
            return; // Invalid channel for AEMNet EGT
    }

    CanTxTyped<aemnet::Egt2224Data> frame(id, true);
    for (uint8_t i = 0; i < EGT_CHANNELS; i++)
    {
        if (!cfg->egt[i].ExtraCanChannelEnabled)
            continue;

        frame.get().Egt[i] = getEgtDrivers()[i].temperature * 10;
    }
}

#endif /* EGT_CHANNELS > 0 */

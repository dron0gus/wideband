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

    frame.get().Lambda = GetLambda(ch) * 10000;
    frame.get().Oxygen = 0; // TODO:
    frame.get().SystemVolts = sampler.GetInternalHeaterVoltage() * 10;
    frame.get().Flags =
        ((cfg->sensorType == SensorType::LSU49) ? 0x02 : 0x00) |
        ((LambdaIsValid(ch, lambda)) ? 0x80 : 0x00);
    frame.get().Faults = 0; //TODO:
}

#if (EGT_CHANNELS > 0)

namespace aemnet
{

#define AEMNET_EGT_TX_PERIOD        50

// That's for AEM EGT Gauge 1800F (30-0305)
// It does not support multiple EGT channels, as 0x000A0306 is a ID of AEM Boost Gauge 50psia (30-0306)
//#define AEMNET_EGT_BASE_ID          0x000A0305 

// This one is for 8-Channel K-Type CAN Module (30-2224), which supports up to 8 EGT channels
// https://documents.aemelectronics.com/techlibrary_30-2224-_8-channel-k-type-can-module-instructions.pdf
#define AEMNET_EGT1_BASE_ID          0x0000BA00
#define AEMNET_EGT2_BASE_ID          0x0000BB00

// 29 bit ID, 500kbs, rate 20 hz, endian big, DLC 8
// UNIT1 ID: 0x0000BA00 .. 0x0000BA01
// UNIT2 ID: 0x0000BB00 .. 0x0000BB01
struct EgtData
{
    beint16_t Egt[4]; // 0.1 C/bit, -3276.8 to 3276.7 C
} __attribute__((packed));

static_assert(sizeof(EgtData) == 8);

// UNIT1 ID: 0x0000BA02
// UNIT2 ID: 0x0000BB02
struct EgtStatus
{
    beuint16_t ColdJunctionTemp; // 0.1 C/bit, -3276.8 to 3276.7 C
    uint8_t BatteryVoltage;      // 0 - 25.5 V
    uint8_t Reserved[3];
} __attribute__((packed));

static_assert(sizeof(EgtStatus) == 8);

} //namespace aemnet

void SendAemNetEGTFormat(Configuration* cfg, uint8_t ch)
{
    if (ch != 0)
        return; // Use a first channel for config, as AEMNet sends up to 4 EGT channels in one message

    uint32_t id;
    switch (cfg->egt[ch].ExtraCanIdOffset)
    {
        case 0:
            id = AEMNET_EGT1_BASE_ID;
            break;
        case 1:
            id = AEMNET_EGT2_BASE_ID;
            break;
        
        default:
            return; // Invalid channel for AEMNet EGT
    }

    CanTxTyped<aemnet::EgtData> frame(id, true);
    for (uint8_t i = 0; i < EGT_CHANNELS; i++)
    {
        frame.get().Egt[i] = getEgtDrivers()[i].temperature * 10;
    }
}

#endif /* EGT_CHANNELS > 0 */

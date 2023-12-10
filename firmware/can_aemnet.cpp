#include "can.h"
#include "hal.h"

#include "can_helper.h"
#include "can_aemnet.h"

#include "port.h"
#include "fault.h"
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

#define AEMNET_EGT_TX_PERIOD        50
#define AEMNET_EGT_BASE_ID          0x000A0305

// 29 bit ID, 500kbs, rate 20 hz, endian big, DLC 8
// ID: 0x000A0305
struct EgtData
{
    // 1 degC/bit, 0 to 65535 degC
    beuint16_t TemperatureC;
    uint8_t pad[6];
} __attribute__((packed));

static_assert(sizeof(EgtData) == 8);

} //namespace aemnet

static int LambdaIsValid(int ch)
{
    const auto& sampler = GetSampler(ch);
    const auto& heater = GetHeaterController(ch);

    float nernstDc = sampler.GetNernstDc();

    return ((heater.IsRunningClosedLoop()) &&
            (nernstDc > (NERNST_TARGET - 0.1f)) &&
            (nernstDc < (NERNST_TARGET + 0.1f)));
}

void SendAemNetUEGOFormat(uint8_t ch)
{
	Configuration* configuration = GetConfiguration();
    auto id = AEMNET_UEGO_BASE_ID + configuration->afr[ch].AemNetIdOffset;

    const auto& sampler = GetSampler(ch);

    if (configuration->afr[ch].AemNetTx) {
        CanTxTyped<aemnet::UEGOData> frame(id, true);

        frame.get().Lambda = GetLambda(ch) * 10000;
        frame.get().Oxygen = 0; // TODO:
        frame.get().SystemVolts = sampler.GetInternalHeaterVoltage() * 10;
        frame.get().Flags =
            ((configuration->sensorType == SensorType::LSU49) ? 0x02 : 0x00) |
            ((LambdaIsValid(ch)) ? 0x80 : 0x00);
        frame.get().Faults = 0; //TODO:
    }
}

#if (EGT_CHANNELS > 0)
void SendAemNetEGTFormat(uint8_t ch)
{
	Configuration* configuration = GetConfiguration();
    auto id = AEMNET_EGT_BASE_ID + configuration->egt[ch].AemNetIdOffset;

    if (configuration->egt[ch].AemNetTx) {
        CanTxTyped<aemnet::EgtData> frame(id, true);

        frame.get().TemperatureC = getEgtDrivers()[ch].temperature;
    }
}
#endif /* EGT_CHANNELS > 0 */

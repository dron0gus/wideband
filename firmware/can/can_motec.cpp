#include "can.h"
#include "hal.h"

#include "can_motec.h"

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

// MOTEC AFR protocol
// ID: 0x460..0x47F; Offset 0-31
// LTC can output data to any custom address
#define MOTEC_LTC_BASE_ID         0x460

namespace motec
{

enum class SensorStateType : uint8_t {
    Start = 0,
    Diagnostics = 1,
    PreCal = 2,
    Calibration = 3,
    PostCal = 4,
    Paused = 5,
    Heating = 6,
    Running = 7,
    Cooling = 8,
};

struct AfrData1
{
    uint8_t CompoundId = 0; // Message 1 Constant

    beuint16_t Lambda;      // x.xxx La
    beuint16_t Ipn;         // xxxx uA; Normalized Pump cell current
    uint8_t InternalTemp;   // xxx C; LTC Internal Temperature
    
    uint8_t HeaterShortToGnd : 1;
    uint8_t HeaterShortToBatt : 1;
    uint8_t HeaterOpen : 1;
    uint8_t SensorFailedToHeat : 1;
    uint8_t SensorWireShort : 1;
    uint8_t Reserved : 3;

    uint8_t DutyCycle;       // xxx%
} __attribute__((packed));

static_assert(sizeof(AfrData1) == 8);

struct AfrData2
{
    uint8_t CompoundId = 1; // Message 2 Constant
    SensorStateType SensorState;
    beuint16_t VBatt;       // x.xx V
    beuint16_t Ip;          // xxxx uA; Raw Pump cell current
    beuint16_t Ri;          // xxxx Ohms; Sensor cell impedance; 0 for LTC-N, not applicable
} __attribute__((packed));

static_assert(sizeof(AfrData2) == 8);

struct AfrData3
{
    uint8_t CompoundId = 2; // Message 3 Constant
    uint8_t FirmwareVersionLetter; // A:0, B:1, C:2
    beuint16_t FirmwareVersionNumber; // xxx
    beuint16_t SerialNumber;
    uint8_t Reserved[2];
} __attribute__((packed));

static_assert(sizeof(AfrData3) == 8);

} //namespace motec

void SendMotecAfrFormat(Configuration* configuration, uint8_t ch)
{
    auto id = MOTEC_LTC_BASE_ID + configuration->afr[ch].ExtraCanIdOffset;

    const auto& sampler = GetSampler(ch);
    const auto& heater = GetHeaterController(ch);

    CanTxTyped<motec::AfrData1> frame(id, true);
    float lambda = GetLambda(ch);
    frame->Lambda = LambdaIsValid(ch, lambda) ? lambda * 1000 : 0;

    frame->InternalTemp = 35; // TODO: Send actual unit temperature
    frame->DutyCycle = 25; // TODO: Send actual duty cycle
    frame->Ipn = sampler.GetPumpNominalCurrent() * 1000; // TODO: Is that normalized pump current or raw pump current?

    auto status = GetCurrentStatus(ch);
    if (status == wbo::Status::SensorDidntHeat) {
        frame->SensorFailedToHeat = 1;
    }

    // //sampler.GetSensorTemperature(); // TODO: Is that controller temp or sensor temp?

    CanTxTyped<motec::AfrData2> frame2(id, true);

    // TODO: Confirm correct mapping of states
    switch (heater.GetHeaterState()) {
        case HeaterState::Preheat:
        case HeaterState::WarmupRamp:
            frame2->SensorState = motec::SensorStateType::Heating;
            break;
        case HeaterState::ClosedLoop:
            frame2->SensorState = motec::SensorStateType::Running;
            break;
        case HeaterState::Stopped:
        default:
            frame2->SensorState = motec::SensorStateType::Paused;
            break;
    }

    frame2->VBatt = sampler.GetInternalHeaterVoltage() * 100;
    frame2->Ip    = sampler.GetPumpNominalCurrent() * 1000; // TODO: Confirm raw vs normalized pump current
    frame2->Ri    = sampler.GetSensorInternalResistance();
}

#if (EGT_CHANNELS > 0)

// BASE ID: 0x0F0; 0x0F4; 0x0F8; 0x0FC
#define MOTEC_E888_BASE_ID         0x0F0

namespace motec
{

// MOTEC E888 protocol
// AVx =  0.001 V
// TCx = 0.25 C, Signed
// Freqx = 0.1 Hz, Signed
struct E888Data1
{
    uint8_t CompoundId : 3; //   0,   1,   2,     3,     4
    // TODO: Check and handle endianness. Value1 can't be beint16_t because it does not support bit-packing
    uint16_t Value1 : 13; // AV1, AV3, AV5,   AV7,   AV8
    beint16_t Value2;      // AV2, AV4, AV6,   TC7,   TC8
    beint16_t Value3;      // TC1, TC3, TC5, Freq1, Freq2
    beint16_t Value4;      // TC2, TC4, TC6, Freq3, Freq4
} __attribute__((packed));

static_assert(sizeof(E888Data1) == 8);

} //namespace motec

void SendMotecEgtFormat(Configuration* configuration, uint8_t ch)
{
    if (ch % 2 > 0) { // Up to 2 channels per message
        return;
    }

    auto id = MOTEC_E888_BASE_ID + configuration->egt[ch].ExtraCanIdOffset;
    const auto egtDrivers = getEgtDrivers();

    CanTxTyped<motec::E888Data1> frame(id, true);

    switch (ch) {
        case 0: // Channels 1 and 2
            frame->CompoundId = 0;
            frame->Value1 = egtDrivers[ch].temperature * 4;
            if (ch + 1 < EGT_CHANNELS) {
                frame->Value2 = egtDrivers[ch + 1].temperature * 4;
            }
            break;
        // TODO: Add more channels if needed
    }

    frame->CompoundId = ch / 2; // 0-based channel index
}

#endif



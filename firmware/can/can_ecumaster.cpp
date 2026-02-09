#include "can.h"
#include "hal.h"

#include "util/byteswap.h"
#include "can_helper.h"
#include "can_ecumaster.h"

#include "port.h"
#include "status.h"
#include "can_helper.h"
#include "sampling.h"
#include "pump_dac.h"
#include "heater_control.h"
#include "lambda_conversion.h"
#include "max3185x.h"
#include "../for_rusefi/wideband_can.h"

// EcuMaster protocol
// CAN 1Mbps, big-endian

#define ECUMASTER_L2C_TX_PERIOD_MS    10
#define ECUMASTER_L2C_BASE_ID         0x664

namespace ecumaster
{

enum class CalibrationStates : uint8_t {
    Start = 0x00,
    WaitForSPIReset = 0x04,
    Finished = 0x08,
    Error = 0x0C,
};

enum class SensorType : uint8_t {
    LSU42 = 0,
    LSU49 = 1,
    LSUADV = 2,
};

// BASE_ID + 0
struct AfrData1
{
    beuint16_t SystemVolts;     // 0.00-655.35 V; 1/100
    uint8_t HeaterPower;        // 0-100 %; 100/255
    uint8_t SensorTemp;         // 0-1020 C; 4/1
    beuint16_t Lambda;          // 0.001 Lambda/bit, 0.000 to 65.535 Lambda
    uint8_t VmShortVcc : 1;
    uint8_t VmShortGnd : 1;
    uint8_t UnShortVcc : 1;
    uint8_t UnShortGnd : 1;
    uint8_t IaIpShortVcc : 1;
    uint8_t IaIpShortGnd : 1;
    uint8_t VubLowVoltage : 1;
    uint8_t HeaterShortVcc : 1;
    uint8_t HeaterShortGnd : 1;
    uint8_t HeaterOpenLoad : 1;
    CalibrationStates CalibrationState : 3;
    SensorType DeviceVersion : 3;
} __attribute__((packed));

static_assert(sizeof(AfrData1) == 8);

// BASE_ID + 1
struct AfrData2
{
    beint16_t IpCurrent;               // -32.768 to 32.767 mA; 1/1000
    beint16_t OxygenConcentration;     // -327.68 to 327.67 %; 1/100
    beuint16_t Ri;                     //  0.0 to 6553.5 Ohm; 1/10
    uint8_t Reserved[2];
} __attribute__((packed));

static_assert(sizeof(AfrData2) == 8);

} //namespace ecumaster


void SendEcuMasterAfrFormat(Configuration* configuration, uint8_t ch)
{
    auto id = ECUMASTER_L2C_BASE_ID + configuration->afr[ch].ExtraCanIdOffset * 2;
    const auto& sampler = GetSampler(ch);

    CanTxTyped<ecumaster::AfrData1> frame(id, true);
    frame->SystemVolts = sampler.GetInternalHeaterVoltage() * 10;
    frame->SensorTemp = sampler.GetSensorTemperature() / 4;
    frame->HeaterPower = 0; // TODO: Retrieve actual duty cycle

    float lambda = GetLambda(ch);
    frame->Lambda = LambdaIsValid(ch, lambda) ? lambda * 1000 : 0;

    frame->CalibrationState = ecumaster::CalibrationStates::Finished;
    switch (configuration->sensorType) {
        case SensorType::LSU42:
            frame->DeviceVersion = ecumaster::SensorType::LSU42;
            break;
        case SensorType::LSU49:
            frame->DeviceVersion = ecumaster::SensorType::LSU49;
            break;
        case SensorType::LSUADV:
            frame->DeviceVersion = ecumaster::SensorType::LSUADV;
            break;
    }

    CanTxTyped<ecumaster::AfrData2> frame2(id + 1, true);
    frame2->IpCurrent = sampler.GetPumpNominalCurrent() * 1000;
    
    // Calculate oxygen concentration from lambda
    // O2% = (lambda - 1) / lambda * 20.95 (atmospheric oxygen percentage)
    float oxygenPercent = 0;
    if (LambdaIsValid(ch, lambda) && lambda > 0) {
        oxygenPercent = ((lambda - 1.0f) / lambda) * 20.95f;
    }
    frame2->OxygenConcentration = oxygenPercent * 100; // Convert to 1/100 scaling
    
    frame2->Ri = sampler.GetSensorInternalResistance() * 10;
}

#if (EGT_CHANNELS > 0)

namespace ecumaster
{
    
// Offset: 0, 1
#define ECUMASTER_CLASSIC_EGT_BASE_ID         0x610
#define ECUMASTER_BLACK_EGT_BASE_ID           0x660

struct EgtData
{
    beint16_t Egt[4];
} __attribute__((packed));

static_assert(sizeof(EgtData) == 8);

} //namespace ecumaster


void SendEcuMasterEgtFormat(Configuration* configuration, uint8_t ch)
{
    if (ch != 0)
        return; // EcuMaster protocol sends 1-4 channels in one message

    auto base = ECUMASTER_CLASSIC_EGT_BASE_ID;
    if (configuration->egt[ch].ExtraCanProtocol == CanProtocol::EcuMasterBlack)
        base = ECUMASTER_BLACK_EGT_BASE_ID;

    auto id = base + configuration->egt[ch].ExtraCanIdOffset;

    CanTxTyped<ecumaster::EgtData> frame(id, true);

    for (uint8_t i = 0; i < EGT_CHANNELS; i++)
    {
        frame.get().Egt[i] = getEgtDrivers()[i].temperature;
    }
}

#endif
#include "can.h"
#include "hal.h"

#include "can_emtron.h"

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

// EMTRON AFR protocol

#define EMTRON_ELC_BASE_ID         0x28F
// BASE_ID = 671
// ID = BASE_ID + Offset (0..10)
// 11 Bit ID, 8 bytes, 100Hz

namespace emtron
{
    // Bit 0/1: Virtual Ground status
    enum class VirtualGroundStatus : uint8_t {
        ShortToGround = 0,
        IcPowerSupplyLow = 1,
        ShortToVbatt = 2,
        Ok = 3
    };

    // Bit 2/3: Nernst Cell status
    enum class NernstCellStatus : uint8_t {
        ShortToGround = 0,
        IcPowerSupplyLow = 1,
        ShortToVbatt = 2,
        Ok = 3
    };

    // Bit 4/5: Pump Current status
    enum class PumpCurrentStatus : uint8_t {
        ShortToGround = 0,
        IcPowerSupplyLow = 1,
        ShortToVbatt = 2,
        Ok = 3
    };

    // Bit 6/7: Heater status
    enum class HeaterStatus : uint8_t {
        ShortToGround = 0,
        IcOpenLoad = 1,
        ShortToVbatt = 2,
        Ok = 3
    };

    enum class AfrStatus : uint8_t {
        Off = 0,
        NormalOperation = 1,
        SensorWarmingUp = 2,
        RpmLockout = 3,
        PostStartLockout = 4,
        ReadingCalibrationData = 5,
        
        HeaterUnderTemperature = 14,  // Cannot reach 650 DegC
        HeaterOverTemperature = 15,
        SensorShutdownThermalShock = 16,
        CannotReadChipId = 17,
        SetPumpReferenceCommandInvalid = 18,
        CalibrateCommandInvalid = 19,
        StandaloneCommandInvalid = 20,
        NernstCalDataInvalid = 21,
        PumpCalDataInvalid = 22,
        
        LambdaStabilityError = 19,
        ErrorReadingChipId = 20,
        SystemVoltageLow = 22,
        CannotEnterCalibrationMode = 22,
        CannotEnterStandaloneMode = 23
    };

    struct AfrData
    {
        uint8_t FrameIndex = 0;
        beuint16_t Lambda;      // 0.001 Lambda
        beuint16_t PumpCurrent; // 0.001 mA
        
        VirtualGroundStatus VirtualGroundFault : 2;
        NernstCellStatus NernstCellFault : 2;
        PumpCurrentStatus PumpCurrentFault : 2;
        HeaterStatus HeaterFault : 2;

        AfrStatus Status;

        uint8_t HeaterDuty;     // 0-100%
    } __attribute__((packed));

    static_assert(sizeof(AfrData) == 8);

} //namespace emtron

void SendEmtronAfrFormat(Configuration* configuration, uint8_t ch)
{
    auto id = EMTRON_ELC_BASE_ID + configuration->afr[ch].ExtraCanIdOffset;
    const auto& sampler = GetSampler(ch);

    CanTxTyped<emtron::AfrData> frame(id, true);
    float lambda = GetLambda(ch);
    frame->Lambda = LambdaIsValid(ch, lambda) ? lambda * 1000 : 0;

    frame->PumpCurrent = sampler.GetPumpNominalCurrent() * 1000;

    switch (GetCurrentStatus(ch)) {
        case wbo::Status::Preheat:
            frame->Status = emtron::AfrStatus::SensorWarmingUp;
            break;
        case wbo::Status::Warmup:
            frame->Status = emtron::AfrStatus::SensorWarmingUp;
            break;
        case wbo::Status::RunningClosedLoop:
            frame->Status = emtron::AfrStatus::NormalOperation;
            break;
        case wbo::Status::SensorDidntHeat:
            frame->Status = emtron::AfrStatus::HeaterUnderTemperature;
            break;
        case wbo::Status::SensorOverheat:
            frame->Status = emtron::AfrStatus::HeaterOverTemperature;
            break;
        case wbo::Status::SensorUnderheat:
            frame->Status = emtron::AfrStatus::HeaterUnderTemperature;
            break;
    }

    // TODO: Detect actual faults
    frame->VirtualGroundFault = emtron::VirtualGroundStatus::Ok;
    frame->NernstCellFault = emtron::NernstCellStatus::Ok;
    frame->PumpCurrentFault = emtron::PumpCurrentStatus::Ok;
    frame->HeaterFault = emtron::HeaterStatus::Ok;

    frame->HeaterDuty = 25; // TODO: Send actual duty cycle
}

#if (EGT_CHANNELS > 0)

#define EMTRON_ETC4_BASE_ID         0x2B3

namespace emtron
{

// Base ID: 0x2B3
// Device offset: 0..3
struct ETC4Data
{
    // TODO: Check and handle endianness. These can't be beint16_t because they do not support bit-packing
    uint16_t Egt1 : 12; // x1 -50 C Offset
    uint16_t Egt2 : 12;
    uint16_t Egt3 : 12;
    uint16_t Egt4 : 12;
    uint8_t ColdJunctionTemp; // 0-255 C
    uint8_t Reserved;

} __attribute__((packed));

static_assert(sizeof(ETC4Data) == 8);

} //namespace emtron

void SendEmtronEgtFormat(Configuration* configuration, uint8_t ch)
{
    if (ch != 0)
        return;

    auto id = EMTRON_ETC4_BASE_ID + configuration->egt[ch].ExtraCanIdOffset;

    const auto egtDrivers = getEgtDrivers();

    CanTxTyped<emtron::ETC4Data> frame(id, true);
    frame->Egt1 = egtDrivers[0].temperature + 50;

    #if (EGT_CHANNELS > 1)
        frame->Egt2 = egtDrivers[1].temperature + 50;
    #endif

    #if (EGT_CHANNELS > 2)
        frame->Egt3 = egtDrivers[2].temperature + 50;
    #endif

    #if (EGT_CHANNELS > 3)
        frame->Egt4 = egtDrivers[3].temperature + 50;
    #endif

    frame->ColdJunctionTemp = egtDrivers[0].coldJunctionTemperature;
}

#endif

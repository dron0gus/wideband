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

#define EMTRON_ELC_BASE_ID         0x29F
// BASE_ID = 671
// ID = BASE_ID + Offset (0..11)
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
        
        /* Overlapping values in docs, now sure if correct */
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

    frame->HeaterDuty = GetHeaterDuty(ch) * 100;
}

#if (EGT_CHANNELS > 0)

#define EMTRON_ETC4_BASE_ID         0x2B3

namespace emtron
{

// Base ID: 0x2B3
// Device offset: 0..2
struct ETC4Data
{
    // Manual packing of 12-bit big-endian values
    uint8_t EgtData [6];

    uint8_t ColdJunctionTemp; // 0-255 C
    uint8_t Reserved;

} __attribute__((packed));

static_assert(sizeof(ETC4Data) == 8);

uint16_t get_temperature(float raw_temp)
{
    int16_t temp = static_cast<int16_t>(raw_temp + 50.0f); // Apply -50 C offset
    
    if (temp < 0) temp = 0; // Clamp to 0 if somehow reading is below -50 C
    if (temp > 4095) temp = 4095; // 12-bit limit

    return temp;
}

} //namespace emtron

void SendEmtronEgtFormat(Configuration* configuration)
{
    auto id = EMTRON_ETC4_BASE_ID + configuration->egt[0].ExtraCanIdOffset;
    CanTxTyped<emtron::ETC4Data> frame(id, true);

    const auto egtDrivers = getEgtDrivers();
    
    uint16_t temp;

    if (configuration->egt[0].ExtraCanChannelEnabled)
    {
        uint16_t temp = emtron::get_temperature(egtDrivers[0].temperature);
        frame->EgtData[0] = (temp >> 4) & 0xFF; // High 8 bits
        frame->EgtData[1] = (temp & 0xF) << 4; // Low 4 bits in high nibble
    }

    #if (EGT_CHANNELS > 1)
    if (configuration->egt[1].ExtraCanChannelEnabled)
    {
        temp = emtron::get_temperature(egtDrivers[1].temperature);
        frame->EgtData[1] |= (temp >> 8) & 0xF; // High 4 bits in low nibble
        frame->EgtData[2] = temp & 0xFF; // Low 8 bits
    }
    #endif

    #if (EGT_CHANNELS > 2)
    if (configuration->egt[2].ExtraCanChannelEnabled)
    {
        temp = emtron::get_temperature(egtDrivers[2].temperature);
        frame->EgtData[3] |= (temp >> 8) & 0xF; // High 4 bits in low nibble
        frame->EgtData[4] = temp & 0xFF; // Low 8 bits
    }
    #endif

    #if (EGT_CHANNELS > 3)
    if (configuration->egt[3].ExtraCanChannelEnabled)
    {
        temp = emtron::get_temperature(egtDrivers[3].temperature);
        frame->EgtData[4] |= (temp >> 8) & 0xF; // High 4 bits in low nibble
        frame->EgtData[5] = temp & 0xFF; // Low 8 bits
    }
    #endif

    frame->ColdJunctionTemp = egtDrivers[0].coldJunctionTemperature;
}

#endif

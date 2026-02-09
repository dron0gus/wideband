#include "can.h"
#include "hal.h"

#include "can_link.h"

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

// Link ECU protocol
// 1Mbps default, big endian, DLC 8

#define LINKECU_L2C_TX_PERIOD_MS    10
#define LINKECU_L2C_BASE_ID         0x3B6

#define LINKECU_L2C_IN_BASE_ID      0x3BE
#define LINKECU_L2C_SET_IDX_ID      0x3BC

namespace linkecu
{

enum class AfrStatus : uint8_t {
    Off = 0,
    Disabled = 1,
    Initializing = 2,
    Diagnostic = 3,
    Calibration = 4,
    Heating = 5,
    Operating = 6,
};


// ID: 0x3B6..0x3BD for AFR channels
struct AfrData1
{
    uint8_t FrameIndex = 50; // Fixed value for AFR frames

    uint8_t ErrorCodes;
    beuint16_t Lambda;      // 0.001 Lambda/bit, 0.000 to 65.535 Lambda
    beuint16_t SensorTemp;  // 0-65535 C; 1/1

    AfrStatus Status;

    uint8_t Reserved;
} __attribute__((packed));

static_assert(sizeof(AfrData1) == 8);

struct AfrData2
{
    uint8_t FrameIndex = 51; // Fixed value for AFR frames
    uint8_t Reserved;

    beuint16_t IpCurrent;       // -32.768 to 32.767 mA; 1/1000
    beuint16_t SystemVoltage;   // 0.00-655.35 V; 1/100
    beuint16_t HeaterVoltage;   // 0.00-655.35 V; 1/100
} __attribute__((packed));

static_assert(sizeof(AfrData2) == 8);

} //namespace linkecu


static void SendAck(uint16_t id, uint8_t id_ok, uint8_t bus_freq_ok)
{
    CANTxFrame frame;

#ifdef STM32G4XX
    frame.common.RTR = 0;
#else // Not CAN FD
    frame.RTR = CAN_RTR_DATA;
#endif

    frame.DLC = 8;

    CAN_EXT(frame) = 1;
    CAN_EID(frame) = LINKECU_L2C_BASE_ID + id;

    frame.data8[0] = 24;
    frame.data8[1] = id_ok ? 0x01 : 0xFF;
    frame.data8[2] = bus_freq_ok ? 0x01 : 0xFF;

    canTransmitTimeout(&CAND1, CAN_ANY_MAILBOX, &frame, TIME_INFINITE);
}

void SendLinkAfrFormat(Configuration* configuration, uint8_t ch)
{
    auto id = LINKECU_L2C_BASE_ID + configuration->afr[ch].ExtraCanIdOffset;
    const auto& sampler = GetSampler(ch);
    const auto& heater = GetHeaterController(ch);

    CanTxTyped<linkecu::AfrData1> frame(id, true);
    float lambda = GetLambda(ch);
    frame->Lambda = LambdaIsValid(ch, lambda) ? lambda * 1000 : 0;
    frame->SensorTemp = sampler.GetSensorTemperature();

    switch (heater.GetHeaterState()) {
        case HeaterState::Preheat:
        case HeaterState::WarmupRamp:
            frame->Status = linkecu::AfrStatus::Heating;
            break;
        case HeaterState::ClosedLoop:
            frame->Status = linkecu::AfrStatus::Operating;
            break;
        case HeaterState::Stopped:
        default:
            frame->Status = linkecu::AfrStatus::Disabled;
            break;
    }
    frame->ErrorCodes = 0; // TODO:

    uint8_t deviceType = 0;
    switch (configuration->sensorType) {
        case SensorType::LSU42:
            deviceType = 0;
            break;
        case SensorType::LSU49:
            deviceType = 1;
            break;
        case SensorType::LSUADV:
            deviceType = 2;
            break;
    }

    CanTxTyped<linkecu::AfrData2> frame2(id, true);
    frame2->IpCurrent = sampler.GetPumpNominalCurrent() * 1000;
    frame2->SystemVoltage = sampler.GetInternalHeaterVoltage() * 100;
    frame2->HeaterVoltage = heater.GetHeaterEffectiveVoltage() * 100;
}

#if (EGT_CHANNELS > 0)

#define LINKECU_TCCXX_BASE_ID         0x705
#define LINKECU_TCCXX_DATA_3_ID       0x707
#define LINKECU_TCCXX_STATUS_ID       0x708

namespace linkecu
{

struct EgtData1
{
    beint16_t Egt[4];    // 0 to 1250 C; 1/4
} __attribute__((packed));

static_assert(sizeof(EgtData1) == 8);


struct EgtData3
{
    beint16_t Egt[2];    // 0 to 1250 C; 1/4
    uint16_t Reserved;
    uint8_t SupplyVolt; // 0.0 to 25.5 V; 1/10
    uint8_t ChipTemp;   // 0 to 125 C; 1/1
} __attribute__((packed));

static_assert(sizeof(EgtData3) == 8);

enum struct TcStatus : uint8_t {
    Ok = 0,
    ShortToVcc = 1,
    ShortToGnd = 2,
    NotConnected = 3,
    UnknownError = 4,
    ChipMissing = 5,
};

struct EgtStatus
{
    TcStatus TcStatus1: 4;
    TcStatus TcStatus2: 4;
    TcStatus TcStatus3: 4;
    TcStatus TcStatus4: 4;
    TcStatus TcStatus5: 4;
    TcStatus TcStatus6: 4;
    TcStatus TcStatus7: 4;
    TcStatus TcStatus8: 4;
    TcStatus TcStatus9: 4;
    TcStatus TcStatus10: 4;
    uint8_t Reserved[3];
} __attribute__((packed));

static_assert(sizeof(EgtStatus) == 8);

} //namespace linkecu

void SendLinkEgtFormat(Configuration* configuration, uint8_t ch)
{
    if (ch != 0)
        return; // Link ECU protocol sends 1-4 channels in one message

    const auto& sampler = GetSampler(ch);
    const auto egtDrivers = getEgtDrivers();

    CanTxTyped<linkecu::EgtData1> frame(LINKECU_TCCXX_BASE_ID, true);
    frame->Egt[0] = egtDrivers[0].temperature / 4;

    for (uint8_t i = 1; i < EGT_CHANNELS ; i++)
    {
        frame->Egt[i] = egtDrivers[i].temperature / 4;
    }

    CanTxTyped<linkecu::EgtData3> frame2(LINKECU_TCCXX_DATA_3_ID, true);
    frame2->SupplyVolt = sampler.GetInternalHeaterVoltage() / 10;
    frame2->ChipTemp = GetMcuTemperature();

    CanTxTyped<linkecu::EgtStatus> frame3(LINKECU_TCCXX_STATUS_ID, true);
    // TODO: set actual status
}

#endif

void ProcessLinkCanMessage(const CANRxFrame* frame, Configuration* configuration, struct CanStatusData* statusData)
{
    // Ignore std frames, only listen to ext
    if (!CAN_EXT(*frame))
    {
        return;
    }

    auto id = CAN_ID(*frame);

    if (id == LINKECU_L2C_IN_BASE_ID && frame->DLC == 8 && (frame->data8[0]) == 85)
    {
        // This is status from ECU
        // - Engine speed
        // - Exhaust absolute pressure
        // - Use exhaust pressure

        uint16_t engineRpm = (static_cast<uint16_t>(frame->data8[2]) << 8) | frame->data8[3];
        // Emulate LinkEcu CanLambda
        if (engineRpm > 400)
        {
            statusData->heaterAllow = HeaterAllow::Allowed;
        }
        else if (engineRpm < 10)
        {
            statusData->heaterAllow = HeaterAllow::NotAllowed;
        }

        // x10 kPa (eg 1100 = 110.0kPa)
        // uint16_t exhaustPressure = (static_cast<uint16_t>(frame->data8[4]) << 8) | frame->data8[5];
        // uint8_t useExhaustPressure = frame->data8[6] != 0;
    }
    // Check if it's an "index set" message
    else if (id >= LINKECU_L2C_SET_IDX_ID && id <= LINKECU_L2C_SET_IDX_ID + 7 && frame->DLC == 8 && (frame->data8[0] == 24))
    {
        for (int i = 0; i < AFR_CHANNELS; i++) {
            if (id - configuration->afr[i].ExtraCanIdOffset == LINKECU_L2C_SET_IDX_ID) {
                uint8_t offset = frame->data8[1] & 0x0F;

                // 0 = 100 kbit/s
                // 1 = 125 kbit/s
                // 2 = 250 kbit/s
                // 3 = 500 kbit/s
                // 4 = 1 Mbit/s (default)
                uint8_t bus_freq = frame->data8[1];

                configuration->afr[i].ExtraCanIdOffset = offset;
                SetConfiguration();
                SendAck(id, 1, 0); // id_ok = 1, bus_freq_ok not implemented yet
            }
        }
    }
}
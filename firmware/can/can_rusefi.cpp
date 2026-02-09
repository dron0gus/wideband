#include "can.h"
#include "hal.h"

#include "can_rusefi.h"

#include "util/byteswap.h"
#include "port.h"
#include "status.h"
#include "can_helper.h"
#include "sampling.h"
#include "pump_dac.h"
#include "heater_control.h"
#include "pump_control.h"
#include "lambda_conversion.h"

#include <rusefi/math.h>
// this same header is imported by rusEFI to get struct layouts and firmware version
#include "../for_rusefi/wideband_can.h"

static void SendAck()
{
    CANTxFrame frame;

#ifdef STM32G4XX
    frame.common.RTR = 0;
#else // Not CAN FD
    frame.RTR = CAN_RTR_DATA;
#endif

    CAN_EXT(frame) = 1;
    CAN_EID(frame) = WB_ACK;
    frame.DLC = 0;

    canTransmitTimeout(&CAND1, CAN_ANY_MAILBOX, &frame, TIME_INFINITE);
}

void SendRusefiFormat(Configuration* configuration, uint8_t ch)
{
    auto baseAddress = WB_DATA_BASE_ADDR + 2 * configuration->afr[ch].RusEfiIdx;

    const auto& sampler = GetSampler(ch);
    const auto& heater = GetHeaterController(ch);

    auto nernstDc = sampler.GetNernstDc();
    auto pumpDuty = GetPumpOutputDuty(ch);
    auto lambda = GetLambda(ch);

    // Lambda is valid if:
    // 1. Nernst voltage is near target
    // 2. Lambda is >0.6 (sensor isn't specified below that)
    bool lambdaValid = LambdaIsValid(ch, lambda);

    if (configuration->afr[ch].RusEfiTx) {
        CanTxTyped<wbo::StandardData> frame(baseAddress + 0);

        // The same header is imported by the ECU and checked against this data in the frame
        frame.get().Version = RUSEFI_WIDEBAND_VERSION;

        uint16_t lambdaInt = lambdaValid ? (lambda * 10000) : 0;
        frame.get().Lambda = lambdaInt;
        frame.get().TemperatureC = sampler.GetSensorTemperature();
        frame.get().Valid = lambdaValid ? 0x01 : 0x00;
    }

    if (configuration->afr[ch].RusEfiTxDiag) {
        CanTxTyped<wbo::DiagData> frame(baseAddress + 1);;

        frame.get().Esr = sampler.GetSensorInternalResistance();
        frame.get().NernstDc = nernstDc * 1000;
        frame.get().PumpDuty = pumpDuty * 255;
        frame.get().status = GetCurrentStatus(ch);
        frame.get().HeaterDuty = GetHeaterDuty(ch) * 255;
    }
}


#if (EGT_CHANNELS > 0)

void SendRusefiEgtFormat(Configuration* configuration, uint8_t ch)
{
}

#endif

void ProcessRusefiCanMessage(const CANRxFrame* frame, Configuration* configuration, struct CanStatusData* statusData)
{
    // Ignore std frames, only listen to ext
    if (!CAN_EXT(*frame))
    {
        return;
    }

    // Ignore not ours frames
    if (WB_MSG_GET_HEADER(CAN_ID(*frame)) != WB_BL_HEADER)
    {
        return;
    }

    if (frame->DLC >= 2 && CAN_ID(*frame) == WB_MSG_ECU_STATUS)
    {
        // This is status from ECU
        // - battery voltage
        // - heater enable signal
        // - optionally pump control gain

        // data1 contains heater enable bit
        if ((frame->data8[1] & 0x1) == 0x1)
        {
            statusData->heaterAllow = HeaterAllow::Allowed;
        }
        else
        {
            statusData->heaterAllow = HeaterAllow::NotAllowed;
        }

        // data0 contains battery voltage in tenths of a volt
        float vbatt = frame->data8[0] * 0.1f;
        if (vbatt < 5)
        {
            // provided vbatt is bogus, default to 14v nominal
            statusData->remoteBatteryVoltage = 14;
        }
        else
        {
            statusData->remoteBatteryVoltage = vbatt;
        }

        if (frame->DLC >= 3) {
            // data2 contains pump controller gain in percent (0-200)
            float pumpGain = frame->data8[2] * 0.01f;
            SetPumpGainAdjust(clampF(0, pumpGain, 1));
        }
    }
    // If it's a bootloader entry request, reboot to the bootloader!
    else if ((frame->DLC == 0 || frame->DLC == 1) && CAN_ID(*frame) == WB_BL_ENTER)
    {
        // If 0xFF (force update all) or our ID, reset to bootloader, otherwise ignore
        if (frame->DLC == 0 || frame->data8[0] == 0xFF || frame->data8[0] == GetConfiguration()->afr[0].RusEfiIdx)
        {
            SendAck();

            // Let the message get out before we reset the chip
            chThdSleep(50);

            NVIC_SystemReset();
        }
    }
    // Check if it's an "index set" message
    else if (frame->DLC == 1 && CAN_ID(*frame) == WB_MSG_SET_INDEX)
    {
        int offset = frame->data8[0];
        for (int i = 0; i < AFR_CHANNELS; i++) {
            configuration->afr[i].RusEfiIdx = offset + i;
        }
        for (int i = 0; i < EGT_CHANNELS; i++) {
            configuration->egt[i].RusEfiIdx = offset + i;
        }
        SetConfiguration();
        SendAck();
    }
}
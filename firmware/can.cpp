#include "hal.h"

#include "can.h"

#include "fault.h"
#include "can_helper.h"
#include "can/can_rusefi.h"
#include "can/can_aemnet.h"
#include "can/can_ecumaster.h"
#include "can/can_haltech.h"
#include "can/can_link.h"

#include "port.h"

#include <rusefi/math.h>


static Configuration* configuration;

static THD_WORKING_AREA(waCanTxThread, 512);
void CanTxThread(void*)
{
    int cycle = 0;
    chRegSetThreadName("CAN Tx");

    // Current system time.
    systime_t prev = chVTGetSystemTime();

    while(1)
    {
        // AFR - 100 Hz
        for (int ch = 0; ch < AFR_CHANNELS; ch++)
        {
            SendCanForChannel(ch);
        }

        // EGT - 20 Hz
        if ((cycle % 5) == 0) {
            for (int ch = 0; ch < EGT_CHANNELS; ch++) {
                SendCanEgtForChannel(ch);
            }
        }

        cycle++;
        prev = chThdSleepUntilWindowed(prev, chTimeAddX(prev, TIME_MS2I(WBO_TX_PERIOD_MS)));
    }
}

static struct CanStatusData CanStatusData = {
    .heaterAllow = HeaterAllow::Unknown,
    .remoteBatteryVoltage = 0.0f,
};

static THD_WORKING_AREA(waCanRxThread, 512);
void CanRxThread(void*)
{
    chRegSetThreadName("CAN Rx");

    while(1)
    {
        CANRxFrame frame;
        msg_t msg = canReceiveTimeout(&CAND1, CAN_ANY_MAILBOX, &frame, TIME_INFINITE);

        // Ignore non-ok results...
        if (msg != MSG_OK)
        {
            continue;
        }

        ProcessRusefiCanMessage(&frame, configuration, &CanStatusData);
        ProcessLinkCanMessage(&frame, configuration, &CanStatusData);
    }
}

HeaterAllow GetHeaterAllowed()
{
    return CanStatusData.heaterAllow;
}

float GetRemoteBatteryVoltage()
{
    return CanStatusData.remoteBatteryVoltage;
}

void InitCan()
{
    configuration = GetConfiguration();

    canStart(&CAND1, &GetCanConfig(configuration->CanMode));
    chThdCreateStatic(waCanTxThread, sizeof(waCanTxThread), NORMALPRIO, CanTxThread, nullptr);
    chThdCreateStatic(waCanRxThread, sizeof(waCanRxThread), NORMALPRIO - 4, CanRxThread, nullptr);
}


// Weak link so boards can override it
__attribute__((weak)) void SendCanForChannel(uint8_t ch)
{
    SendRusefiFormat(configuration, ch);

    switch (configuration->afr[ch].ExtraCanProtocol)
    {
        case CanProtocol::AemNet:
            SendAemNetUEGOFormat(configuration, ch);
            break;
        case CanProtocol::EcuMasterClassic:
        case CanProtocol::EcuMasterBlack:
            SendEcuMasterAfrFormat(configuration, ch);
            break;
        case CanProtocol::Haltech:
            SendHaltechAfrFormat(configuration, ch);
            break;
        case CanProtocol::LinkEcu:
            SendLinkAfrFormat(configuration, ch);
            break;
        default:
            break;
    }
}

__attribute__((weak)) void SendCanEgtForChannel(uint8_t ch)
{
#if (EGT_CHANNELS > 0)

    SendRusefiEgtFormat(configuration, ch);

    switch (configuration->afr[ch].ExtraCanProtocol)
    {
        case CanProtocol::AemNet:
            SendAemNetEGTFormat(configuration, ch);
            break;
        case CanProtocol::EcuMasterClassic:
        case CanProtocol::EcuMasterBlack:
            SendEcuMasterEgtFormat(configuration, ch);
            break;
        case CanProtocol::Haltech:
            SendHaltechEgtFormat(configuration, ch);
            break;
        case CanProtocol::LinkEcu:
            SendLinkEgtFormat(configuration, ch);
            break;
        default:
            break;
    }
#endif
}

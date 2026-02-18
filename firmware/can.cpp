#include "ch.h"
#include "hal.h"

#include "can.h"

#include "fault.h"
#include "can_helper.h"
#include "can/can_rusefi.h"
#include "can/can_aemnet.h"
#include "can/can_ecumaster.h"
#include "can/can_haltech.h"
#include "can/can_link.h"
#include "can/can_emtron.h"
#include "can/can_motec.h"

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
            SendCanEgt();
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
        case CanAfrProtocol::AemNet:
            SendAemNetUEGOFormat(configuration, ch);
            break;
        case CanAfrProtocol::EcuMaster:
            SendEcuMasterAfrFormat(configuration, ch);
            break;
        case CanAfrProtocol::Haltech:
            SendHaltechAfrFormat(configuration, ch);
            break;
        case CanAfrProtocol::LinkEcu:
            SendLinkAfrFormat(configuration, ch);
            break;
        case CanAfrProtocol::Emtron:
            SendEmtronAfrFormat(configuration, ch);
            break;
        case CanAfrProtocol::Motec:
            SendMotecAfrFormat(configuration, ch);
            break;
        default:
            break;
    }
}

__attribute__((weak)) void SendCanEgt()
{
#if (EGT_CHANNELS > 0)

    SendRusefiEgtFormat(configuration);

    // Look at channel 0 EGT protocol
    switch (configuration->egt[0].ExtraCanProtocol)
    {
        case CanEgtProtocol::AemNet0305:
            SendAemNetEGT0305Format(configuration);
            break;
        case CanEgtProtocol::AemNet2224:
            SendAemNetEGT2224Format(configuration);
            break;
        case CanEgtProtocol::EcuMasterClassic:
        case CanEgtProtocol::EcuMasterBlack:
            SendEcuMasterEgtFormat(configuration);
            break;
        case CanEgtProtocol::Haltech:
            SendHaltechEgtFormat(configuration);
            break;
        case CanEgtProtocol::LinkEcu:
            SendLinkEgtFormat(configuration);
            break;
        case CanEgtProtocol::Emtron:
            SendEmtronEgtFormat(configuration);
            break;
        case CanEgtProtocol::Motec:
            SendMotec888Format(configuration);
            break;
        default:
            break;
    }
#endif
}

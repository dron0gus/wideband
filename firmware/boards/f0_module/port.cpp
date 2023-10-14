#include "port.h"
#include "shared/flash.h"
#include "shared/strap_pin.h"

#include "wideband_config.h"

#include "ch.hpp"
#include "hal.h"

#define ADC_CHANNEL_COUNT 3

void PortPrepareAnalogSampling()
{
    adcStart(&ADCD1, nullptr);
}

static adcsample_t adcBuffer[ADC_CHANNEL_COUNT * ADC_OVERSAMPLE];

static chibios_rt::BinarySemaphore adcDoneSemaphore(/* taken =*/ true);

static void adcDoneCallback(ADCDriver*)
{
    chSysLockFromISR();
    adcDoneSemaphore.signalI();
    chSysUnlockFromISR();
}

const ADCConversionGroup convGroup =
{
    false,
    ADC_CHANNEL_COUNT,
    adcDoneCallback,
    nullptr,
    ADC_CFGR1_CONT | ADC_CFGR1_RES_12BIT,                  // CFGR1
    ADC_TR(0, 0),       // TR
    ADC_SMPR_SMP_7P5,      // SMPR
    ADC_CHSELR_CHSEL0 | ADC_CHSELR_CHSEL2 | ADC_CHSELR_CHSEL3
};

static float AverageSamples(adcsample_t* buffer, size_t idx)
{
    uint32_t sum = 0;

    for (size_t i = 0; i < ADC_OVERSAMPLE; i++)
    {
        sum += buffer[idx];
        idx += ADC_CHANNEL_COUNT;
    }

    constexpr float scale = VCC_VOLTS / (ADC_MAX_COUNT * ADC_OVERSAMPLE);

    return (float)sum * scale;
}

void AnalogSampleStart()
{
    adcStartConversion(&ADCD1, &convGroup, adcBuffer, ADC_OVERSAMPLE);
}

AnalogResult AnalogSampleFinish()
{
    adcDoneSemaphore.wait(TIME_INFINITE);

    return
    {
        .ch =
        {
            {
                .NernstVoltage = AverageSamples(adcBuffer, 0) * (1.0 / NERNST_INPUT_GAIN),
                .PumpCurrentVoltage = AverageSamples(adcBuffer, 1),
                .HeaterSupplyVoltage = 0,
                .NernstClamped = false,
            },
        },
        .VirtualGroundVoltageInt = AverageSamples(adcBuffer, 2),

        // TODO!
        .McuTemp = 0,
    };
}

static size_t boardHwId = 0;

size_t BoardGetHwId()
{
    return boardHwId;
}

extern Configuration __configflash__start__;

int InitConfiguration()
{
    // See https://github.com/mck1117/wideband/issues/11 to explain this madness
    auto sel1 = readSelPin(ID_SEL1_PORT, ID_SEL1_PIN);
    auto sel2 = readSelPin(ID_SEL2_PORT, ID_SEL2_PIN);

    boardHwId = (3 * sel1 + sel2);

    return 0;
}

static Configuration config;

Configuration* GetConfiguration()
{
    const auto& cfg = __configflash__start__;

    // If config has been written before, use the stored configuration
    if (cfg.IsValid())
    {
        // If we have valid config in flash - do not read ID pins, use ID from settings
        config = cfg;
    }
    else
    {
        config.LoadDefaults();

        // Now, override the index with a hardware-strapped option (if present)
        switch (BoardGetHwId()) {
            case 0: config.afr[0].RusEfiIdx = 2; break;
            case 1: config.afr[0].RusEfiIdx = 0; break;
            case 2: config.afr[0].RusEfiIdx = 3; break;
            case 3: config.afr[0].RusEfiIdx = 4; break;
            case 4: /* both floating, do nothing */ break;
            case 5: config.afr[0].RusEfiIdx = 1; break;
            case 6: config.afr[0].RusEfiIdx = 5; break;
            case 7: config.afr[0].RusEfiIdx = 6; break;
            case 8: config.afr[0].RusEfiIdx = 7; break;
            default: break;
        }
    }

    return &config;
}

void SetConfiguration()
{
    // erase config page
    Flash::ErasePage(31);

    // Copy data to flash
    Flash::Write(
        reinterpret_cast<flashaddr_t>(&__configflash__start__),
        reinterpret_cast<const uint8_t*>(&config),
        sizeof(Configuration)
    );
}

SensorType GetSensorType()
{
    return config.sensorType;
}

void SetupESRDriver(SensorType)
{
    // NOP
}

int GetESRSupplyR()
{
    // Nernst AC injection resistor value
    return 22000;
}

void ToggleESRDriver(SensorType)
{
    palTogglePad(NERNST_49_ESR_DRIVER_PORT, NERNST_49_ESR_DRIVER_PIN);
}

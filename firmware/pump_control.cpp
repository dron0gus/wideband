#include "pump_control.h"
#include "wideband_config.h"
#include "heater_control.h"
#include "sampling.h"
#include "pump_dac.h"
#include "pid.h"

#include "ch.h"

struct pump_control_state {
    Pid pumpPid;
};

PidConfig pumpPidConfig = {
    .kP = 50,
    .kI = 10000,
    .kD = 0,
    .clamp = 10,
};

static struct pump_control_state state[AFR_CHANNELS] =
{
    {
        Pid(pumpPidConfig, PUMP_CONTROL_PERIOD),
    },
#if (AFR_CHANNELS >= 2)
    {
        Pid(pumpPidConfig, PUMP_CONTROL_PERIOD),
    },
#endif
#if (AFR_CHANNELS >= 3)
    {
        Pid(pumpPidConfig, PUMP_CONTROL_PERIOD),
    },
#endif
#if (AFR_CHANNELS >= 4)
    {
        Pid(pumpPidConfig, PUMP_CONTROL_PERIOD),
    },
#endif
};

static float pumpGainAdjust = 1.0f;

void SetPumpGainAdjust(float ratio)
{
    pumpGainAdjust = ratio;
}

constexpr float f_abs(float x)
{
    return x > 0 ? x : -x;
}

class SensorDetector
{
public:
    void feed(int pumpCh, const ISampler& sampler)
    {
        if (cycle < 25)
        {
            SetPumpCurrentTarget(pumpCh, 1 * 1000);
            nernstHi = sampler.GetNernstDc();
        }
        else
        {
            SetPumpCurrentTarget(pumpCh, -1 * 1000);
            nernstLo = sampler.GetNernstDc();
        }
        if (++cycle >= 50)
        {
            float amplitude = f_abs(nernstHi - nernstLo);
            if (amplitude > maxAmplitude) {
                maxAmplitude = amplitude;
            }
            cycle = 0;
            counter++;
        }
    }
    void reset()
    {
        cycle = counter = 0;
        nernstHi = nernstLo = 0.0;
        maxAmplitude = 0.0;
    }

private:
    int cycle = 0;
    int counter = 0;
    float nernstHi = 0.0;
    float nernstLo = 0.0;
    float maxAmplitude = 0.0;
};

SensorDetector sensorDetector[AFR_CHANNELS];

static THD_WORKING_AREA(waPumpThread, 256);
static void PumpThread(void*)
{
    chRegSetThreadName("Pump");

    while(true)
    {
        for (int ch = 0; ch < AFR_CHANNELS; ch++)
        {
            pump_control_state &s = state[ch];

            const auto& sampler = GetSampler(ch);
            const auto& heater = GetHeaterController(ch);

            // Only actuate pump when hot enough to not hurt the sensor
            if (heater.IsRunningClosedLoop() ||
                (sampler.GetSensorTemperature() >= heater.GetTargetTemp() - START_PUMP_TEMP_OFFSET))
            {
                float nernstVoltage = sampler.GetNernstDc();

                float result = pumpGainAdjust * s.pumpPid.GetOutput(NERNST_TARGET, nernstVoltage);

                // result is in mA
                SetPumpCurrentTarget(ch, result * 1000);
            }
            else if (sampler.GetSensorTemperature() >= heater.GetTargetTemp() - START_SENSOR_DETECTION_TEMP_OFFSET)
            {
                sensorDetector[ch].feed(ch, sampler);
            }
            else
            {
                // reset sensor detector
                sensorDetector[ch].reset();
                // Otherwise set zero pump current to avoid damaging the sensor
                SetPumpCurrentTarget(ch, 0);
            }
        }

        // Run at 500hz
        chThdSleepMilliseconds(PUMP_CONTROL_PERIOD);
    }
}

void StartPumpControl()
{
    chThdCreateStatic(waPumpThread, sizeof(waPumpThread), NORMALPRIO + 4, PumpThread, nullptr);
}

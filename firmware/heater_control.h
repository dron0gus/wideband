#pragma once

#include <cstdint>

#include "wideband_config.h"

#include "can.h"
#include "pid.h"
#include "timer.h"
#include "fixed_point.h"

enum class HeaterState
{
    Preheat,
    WarmupRamp,
    ClosedLoop,
    Stopped,
    NoHeaterSupply,
};

struct HeaterConfig {
    FixedPoint<uint8_t, 10> HeaterSupplyOffVoltage; // in 0.1V steps, 25.5V max
    FixedPoint<uint8_t, 10> HeaterSupplyOnVoltage;  // in 0.1V steps, 25.5V max
    ScaledValue<uint8_t, 5> PreheatTimeSec; // In 5 second steps, 1275s max
    uint8_t pad[5];
} __attribute__((packed));
static_assert(sizeof(HeaterConfig) == 8, "HeaterConfig size incorrect");

struct ISampler;

struct IHeaterController
{
    virtual void Update(const ISampler& sampler, HeaterAllow heaterAllowState) = 0;
    virtual bool IsRunningClosedLoop() const = 0;
    virtual float GetHeaterEffectiveVoltage() const = 0;
    virtual HeaterState GetHeaterState() const = 0;
    virtual float GetTargetTemp() const = 0;
};

class HeaterControllerBase : public IHeaterController
{
public:
    HeaterControllerBase(int ch);
    void Configure(float targetTempC, float targetEsr, struct HeaterConfig* configuration);
    void Update(const ISampler& sampler, HeaterAllow heaterAllowState) override;

    bool IsRunningClosedLoop() const override;
    float GetHeaterEffectiveVoltage() const override;
    HeaterState GetHeaterState() const override;
    float GetTargetTemp() const override;

    virtual void SetDuty(float duty) const = 0;

    bool GetIsHeatingEnabled(HeaterAllow heaterAllowState, float batteryVoltage);

    HeaterState GetNextState(HeaterState currentState, HeaterAllow haeterAllowState, float batteryVoltage, float sensorTemp);
    float GetVoltageForState(HeaterState state, float sensorEsr);

private:
    Pid m_pid;

    float rampVoltage = 0;
    float heaterVoltage = 0;
    HeaterState heaterState = HeaterState::Preheat;
#ifdef HEATER_MAX_DUTY
    int cycle;
#endif

    float m_targetEsr = 0;
    float m_targetTempC = 0;

    const uint8_t ch;

    int m_retryTime = 0;

    Timer m_heaterStableTimer;
    Timer m_preheatTimer;
    Timer m_warmupTimer;
    Timer m_closedLoopStableTimer;
    Timer m_retryTimer;

    // Stores the time since a non-over/underheat condition
    // If the timer reaches a threshold, an over/underheat has
    //    occured
    Timer m_underheatTimer;
    Timer m_overheatTimer;

    struct HeaterConfig* m_configuration;
};

const IHeaterController& GetHeaterController(int ch);

void StartHeaterControl();
float GetHeaterDuty(int ch);
HeaterState GetHeaterState(int ch);
const char* describeHeaterState(HeaterState state);

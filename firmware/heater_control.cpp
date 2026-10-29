#include "heater_control.h"

#include "status.h"
#include "sampling.h"

using namespace wbo;

static const PidConfig heaterPidConfig =
{
    .kP = 0.3f,      // kP
    .kI = 0.3f,      // kI
    .kD = 0.01f,     // kD
    .clamp = 3.0f,      // Integrator clamp (volts)
};

HeaterControllerBase::HeaterControllerBase(int ch, int preheatTimeSec, int warmupTimeSec)
    : m_pid(heaterPidConfig, HEATER_CONTROL_PERIOD)
    , ch(ch)
    , m_preheatTimeSec(preheatTimeSec)
    , m_warmupTimeSec(warmupTimeSec)
{
}

void HeaterControllerBase::Configure(float targetTempC, float targetEsr)
{
    m_targetTempC = targetTempC;
    m_targetEsr = targetEsr;

    m_preheatTimer.reset();
    m_warmupTimer.reset();
    m_heaterStableTimer.reset();
}

bool HeaterControllerBase::IsRunningClosedLoop() const
{
    return heaterState == HeaterState::ClosedLoop;
}

float HeaterControllerBase::GetTargetTemp() const
{
    return m_targetTempC;
}

float HeaterControllerBase::GetHeaterEffectiveVoltage() const
{
    return heaterVoltage;
}

HeaterState HeaterControllerBase::GetHeaterState() const
{
    return heaterState;
}

HeaterState HeaterControllerBase::GetNextState(HeaterState currentState, HeaterAllow heaterAllowState, float heaterSupplyVoltage, float sensorTemp)
{
    bool heaterAllowed = false;

    // Check battery voltage for thresholds only if there is still no command over CAN
    // Still check battery voltage if commanded over can:
    // ECU can be powered from USB and have "Force O2 heating" option enabled while 0 Vbat
    if ((heaterAllowState == HeaterAllow::Unknown) ||
        (heaterAllowState == HeaterAllow::Allowed))
    {
        // measured or received over CAN voltage too low to start heating
        if (heaterSupplyVoltage < HEATER_SUPPLY_OFF_VOLTAGE)
        {
            m_heaterStableTimer.reset();
            heaterAllowed = false;
        }
        else if (heaterSupplyVoltage > HEATER_SUPPLY_ON_VOLTAGE)
        {
            // measured voltage is high enougth to pre-heating, wait some time to stabilize
            heaterAllowed = m_heaterStableTimer.hasElapsedSec(HEATER_SUPPLY_STAB_TIME);
        } else {
            // Hysteresis: do not allow heating, but also do not reset timer
            heaterAllowed = false;
        }
    } else if (heaterAllowState == HeaterAllow::NotAllowed) {
        // we still need some stabilization time
        m_heaterStableTimer.reset();
        heaterAllowed = false;
    }

    if (!heaterAllowed)
    {
        // ECU hasn't allowed preheat yet, reset timer, and force preheat state
        m_preheatTimer.reset();
        SetStatus(ch, Status::Preheat);
        return HeaterState::Preheat;
    }

    float overheatTemp = m_targetTempC + 100;
    float closedLoopTemp = m_targetTempC - 30;
    float underheatTemp = m_targetTempC - 100;

    switch (currentState)
    {
        case HeaterState::Preheat:
            #ifdef HEATER_FAST_HEATING_THRESHOLD_T
            if (sensorTemp >= HEATER_FAST_HEATING_THRESHOLD_T) {
                // if sensor is already hot - we can start from higher heater voltage
                rampVoltage = 9;

                // Reset the timer for the warmup phase
                m_warmupTimer.reset();

                SetStatus(ch, Status::Warmup);
                return HeaterState::WarmupRamp;
            }
            #endif

            // If preheat timeout, or sensor is already hot (engine running?)
            if (m_preheatTimer.hasElapsedSec(m_preheatTimeSec) || sensorTemp > closedLoopTemp)
            {
                // If enough time has elapsed, start the ramp
                // Start the ramp at 7 volts
                rampVoltage = 7;

                // Reset the timer for the warmup phase
                m_warmupTimer.reset();

                SetStatus(ch, Status::Warmup);
                return HeaterState::WarmupRamp;
            }

            // Stay in preheat - wait for time to elapse
            break;
        case HeaterState::WarmupRamp:
            if (sensorTemp > closedLoopTemp)
            {
                SetStatus(ch, Status::RunningClosedLoop);
                return HeaterState::ClosedLoop;
            }
            else if (m_warmupTimer.hasElapsedSec(m_warmupTimeSec))
            {
                SetStatus(ch, Status::SensorDidntHeat);
                return HeaterState::Stopped;
            }

            break;
        case HeaterState::ClosedLoop:
            // Over/under heat timers track how long it's been since
            // temperature was within normal range (then we abort if
            // it's been too long out of range)
            if (sensorTemp <= overheatTemp)
            {
                m_overheatTimer.reset();
            }

            if (sensorTemp >= underheatTemp)
            {
                m_underheatTimer.reset();
            }

            if (m_overheatTimer.hasElapsedSec(0.5f))
            {
                SetStatus(ch, Status::SensorOverheat);
                return HeaterState::Stopped;
            }
            else if (m_underheatTimer.hasElapsedSec(0.5f))
            {
                SetStatus(ch, Status::SensorUnderheat);
                return HeaterState::Stopped;
            }

            break;
        case HeaterState::Stopped:
            // there is not direct exit from this state, but logic will switch to Preheat state in case of:
            // - heaterSupplyVoltage drops below HEATER_SUPPLY_OFF_VOLTAGE
            // - received HeaterAllow::NotAllowed heaterAllowState
            // - F042 only: no CAN communication for CAN_STATUS_MSG_TIMEOUT_MS causing HeaterAllow::Unknown and heaterSupplyVoltage = 0
            break;
    }

    return currentState;
}

float HeaterControllerBase::GetVoltageForState(HeaterState state, float sensorEsr)
{
    switch (state)
    {
        case HeaterState::Preheat:
            // Max allowed during condensation phase (preheat) is 2v
            return 2.0f;
        case HeaterState::WarmupRamp:
            if (rampVoltage < 12)
            {
                // 0.4 volt per second, divided by battery voltage and update rate
                constexpr float rampRateVoltPerSecond = 0.4f;
                constexpr float heaterFrequency = 1000.0f / HEATER_CONTROL_PERIOD;
                rampVoltage += (rampRateVoltPerSecond / heaterFrequency);
            }

            return rampVoltage;
        case HeaterState::ClosedLoop:
            // "nominal" heater voltage is 7.5v, so apply correction around that point (instead of relying on integrator so much)
            // Negated because lower resistance -> hotter

            // TODO: heater PID should operate on temperature, not ESR
            return 7.5f - m_pid.GetOutput(m_targetEsr, sensorEsr);
        case HeaterState::Stopped:
            // Something has gone wrong, turn off the heater.
            return 0;
    }

    // should be unreachable
    return 0;
}

void HeaterControllerBase::Update(const ISampler& sampler, HeaterAllow heaterAllowState)
{
    // Read sensor state
    float sensorEsr = sampler.GetSensorInternalResistance();
    float sensorTemperature = sampler.GetSensorTemperature();

    #if defined(HEATER_INPUT_DIVIDER)
        // if board has ability to measure heater supply localy - use it
        float heaterSupplyVoltage = sampler.GetInternalHeaterVoltage();
    #elif defined(BOARD_HAS_VOLTAGE_SENSE)
        float heaterSupplyVoltage = GetSupplyVoltage();
    #else // not BOARD_HAS_VOLTAGE_SENSE
        // this board rely on measured voltage from ECU
        float heaterSupplyVoltage = GetRemoteBatteryVoltage();
    #endif

    // Run the state machine
    heaterState = GetNextState(heaterState, heaterAllowState, heaterSupplyVoltage, sensorTemperature);
    heaterVoltage = GetVoltageForState(heaterState, sensorEsr);

    // Clamp before squaring: a negative cooling request must never add heat.
    if (heaterVoltage < 0) {
        heaterVoltage = 0;
    }

    // Limit to 12 volts and the available supply.
    if (heaterVoltage > 12) {
        heaterVoltage = 12;
    }
    if (heaterSupplyVoltage < 1.0f) {
        heaterVoltage = 0;
    } else if (heaterVoltage > heaterSupplyVoltage) {
        heaterVoltage = heaterSupplyVoltage;
    }

    // duty = (V_eff / V_batt) ^ 2
    float voltageRatio = (heaterSupplyVoltage < 1.0f) ? 0 : heaterVoltage / heaterSupplyVoltage;
    float duty = voltageRatio * voltageRatio;

    #ifdef HEATER_MAX_DUTY
    cycle++;
    // limit PWM each 10th cycle (2 time per second) to measure heater supply voltage throuth "Heater-"
    if ((cycle % 10) == 0) {
        if (duty > HEATER_MAX_DUTY) {
            duty = HEATER_MAX_DUTY;
        }
    }
    #endif

    // Protect the sensor in case of very high voltage
    if (heaterSupplyVoltage >= 23)
    {
        duty = 0;
        heaterVoltage = 0;
    }

    // Pipe the output to the heater driver
    SetDuty(duty);
}

const char* describeHeaterState(HeaterState state)
{
    switch (state) {
        case HeaterState::Preheat:
            return "Preheat";
        case HeaterState::WarmupRamp:
            return "WarmupRamp";
        case HeaterState::ClosedLoop:
            return "ClosedLoop";
        case HeaterState::Stopped:
            return "Stopped";
    }

    return "Unknown";
}

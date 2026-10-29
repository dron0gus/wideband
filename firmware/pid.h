#pragma once

struct PidConfig
{
    float kP;
    float kI;
    float kD;
    float clamp;
};

class Pid
{
public:
    Pid(const PidConfig& config, float periodMs)
        : m_config(config)
        , m_periodSec(1e-3 * periodMs)
    {
    }

    float GetOutput(float setpoint, float observation);

    void setCoefficients(const PidConfig& config)
    {
        m_config = config;
        // Reset integrator when changing coefficients to prevent windup
        m_integrator = 0;
        m_lastError = 0;
    }

private:
    PidConfig m_config;
    const float m_periodSec;

    float m_lastError = 0;
    float m_integrator = 0;
};

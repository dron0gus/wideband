#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "fixed_point.h"
#include "heater_control.h"
#include "port.h"
#include "sampling.h"

struct MockConfiguration {
    struct HeaterConfig heaterConfig {
        .HeaterSupplyOffVoltage = { 60 }, // 6.0V
        .HeaterSupplyOnVoltage = { 110 }, // 11.0V
        .PreheatTimeSec = { 1 }, // 5 seconds
        .pad = {0},
    };
} mockConfig;

struct MockHeater : public HeaterControllerBase
{
    MockHeater() : HeaterControllerBase(0) {
    }

    MOCK_METHOD(void, SetDuty, (float), (const, override));
};

TEST(HeaterStateOutput, Preheat)
{
    MockHeater dut;
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    // Shouldn't depend upon sensor ESR
    EXPECT_EQ(2.0f, dut.GetVoltageForState(HeaterState::Preheat, 0));
    EXPECT_EQ(2.0f, dut.GetVoltageForState(HeaterState::Preheat, 300));
    EXPECT_EQ(2.0f, dut.GetVoltageForState(HeaterState::Preheat, 1000));
}

TEST(HeaterStateOutput, WarmupRamp)
{
    MockHeater dut;

    // TODO
}

TEST(HeaterStateOutput, ClosedLoop)
{
    MockHeater dut;
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    // At target -> zero output but with 7.5v offset
    EXPECT_EQ(dut.GetVoltageForState(HeaterState::ClosedLoop, 300), 7.5f);

    // Below target -> more voltage
    EXPECT_GT(dut.GetVoltageForState(HeaterState::ClosedLoop, 400), 7.5f);

    // Above target -> less voltage
    EXPECT_LT(dut.GetVoltageForState(HeaterState::ClosedLoop, 200), 7.5f);
}

TEST(HeaterStateOutput, Cases)
{
    MockHeater dut;
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    EXPECT_EQ(0, dut.GetVoltageForState(HeaterState::Stopped, 0));
    EXPECT_EQ(0, dut.GetVoltageForState(HeaterState::NoHeaterSupply, 0));
}

TEST(HeaterStateMachine, PreheatToWarmupTimeout)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    // For a while it should stay in preheat
    Timer::setMockTime(1e6);
    EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 500));
    Timer::setMockTime(2e6);
    EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 500));
    Timer::setMockTime(4.9e6);
    EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 500));

    // Timer expired, transition to warmup ramp
    Timer::setMockTime(5.1e6);
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 500));
}

TEST(HeaterStateMachine, PreheatToWarmupAlreadyWarm)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    // Preheat for a little while
    for (size_t i = 0; i < 10; i++)
    {
        EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 500));
    }

    // Sensor magically warms up, skip to warmup ramp!
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 780));
}

TEST(HeaterStateMachine, WarmupToClosedLoop)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    // Warm up for a little while
    for (size_t i = 0; i < 10; i++)
    {
        EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    }

    // Sensor warms up, time for closed loop!
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 780));
}

TEST(HeaterStateMachine, WarmupTimeout)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    // For a while it should stay in warmup
    Timer::setMockTime(1e6);
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    Timer::setMockTime(2e6);
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    Timer::setMockTime(9.9e6);
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));

    // Warmup times out, sensor transitions to stopped
    Timer::setMockTime(60.1e6);
    EXPECT_EQ(HeaterState::Stopped, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
}

TEST(HeaterStateMachine, ClosedLoop)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    // Check 5 sec stabilization timeout
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 780));
    Timer::advanceMockTime(1e6);
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 1000));
    Timer::advanceMockTime(1e6);
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 200));

    // Temperature is reasonable, stay in closed loop
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 780));
    Timer::advanceMockTime(10e6);
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 780));

    // Allow too hot briefly
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 1000));
    Timer::advanceMockTime(0.1e6);
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 1000));

    // Wait too long, overheat not allowed
    Timer::advanceMockTime(1e6);
    EXPECT_EQ(HeaterState::Stopped, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 1000));

    // Back to normal
    dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 780);
    Timer::advanceMockTime(1e6);
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 780));

    // Allow too cold briefly
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 600));
    Timer::advanceMockTime(0.1e6);
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 600));

    // Wait too long, underheat not allowed
    Timer::advanceMockTime(1e6);
    EXPECT_EQ(HeaterState::Stopped, dut.GetNextState(HeaterState::ClosedLoop, HeaterAllow::Allowed, 12, 600));
}

TEST(HeaterStateMachine, TerminalStates)
{
    MockHeater dut;
    dut.Configure(780, 300, &mockConfig.heaterConfig, SensorType::LSU49);

    EXPECT_EQ(HeaterState::Stopped, dut.GetNextState(HeaterState::Stopped, HeaterAllow::Allowed, 12, 780));
}

struct MockHeaterSampler : public ISampler
{
    MOCK_METHOD(float, GetNernstDc, (), (const, override));
    MOCK_METHOD(float, GetNernstAc, (), (const, override));
    MOCK_METHOD(float, GetNernstV, (), (const, override));
    MOCK_METHOD(float, GetPumpNominalCurrent, (), (const, override));
    MOCK_METHOD(float, GetInternalHeaterVoltage, (), (const, override));
    MOCK_METHOD(float, GetSensorTemperature, (), (const, override));
    MOCK_METHOD(float, GetSensorInternalResistance, (), (const, override));
};

TEST(HeaterOutput, NegativeVoltageDoesNotBecomePositiveDuty)
{
    using namespace testing;
    Timer::setMockTime(0);
    NiceMock<MockHeater> dut;
    NiceMock<MockHeaterSampler> sampler;
    dut.Configure(730, 80, &mockConfig.heaterConfig, SensorType::LSU42);
    ON_CALL(sampler, GetSensorTemperature()).WillByDefault(Return(750));
    ON_CALL(sampler, GetSensorInternalResistance()).WillByDefault(Return(80));
    dut.Update(sampler, HeaterAllow::Allowed); // warmup
    dut.Update(sampler, HeaterAllow::Allowed); // closed loop
    EXPECT_FLOAT_EQ(7.5f, dut.GetHeaterEffectiveVoltage());

    // A sudden lower resistance produces a negative PID voltage request.
    ON_CALL(sampler, GetSensorInternalResistance()).WillByDefault(Return(35));
    EXPECT_CALL(dut, SetDuty(0)).Times(1);
    dut.Update(sampler, HeaterAllow::Allowed);
    EXPECT_FLOAT_EQ(0, dut.GetHeaterEffectiveVoltage());
}

extern float testRemoteBatteryVoltage;

TEST(HeaterOutput, VoltageAndTelemetryAreLimitedToSupply)
{
    using namespace testing;
    Timer::setMockTime(0);
    NiceMock<MockHeater> dut;
    NiceMock<MockHeaterSampler> sampler;
    dut.Configure(730, 80, &mockConfig.heaterConfig, SensorType::LSU42);
    ON_CALL(sampler, GetSensorTemperature()).WillByDefault(Return(750));
    ON_CALL(sampler, GetSensorInternalResistance()).WillByDefault(Return(200));
    testRemoteBatteryVoltage = 10;
    dut.Update(sampler, HeaterAllow::Allowed);
    EXPECT_CALL(dut, SetDuty(1)).Times(1);
    dut.Update(sampler, HeaterAllow::Allowed);
    EXPECT_FLOAT_EQ(10, dut.GetHeaterEffectiveVoltage());

    testRemoteBatteryVoltage = 24;
    EXPECT_CALL(dut, SetDuty(0)).Times(1);
    dut.Update(sampler, HeaterAllow::Allowed);
    EXPECT_FLOAT_EQ(0, dut.GetHeaterEffectiveVoltage());
    testRemoteBatteryVoltage = 13.7f;
}

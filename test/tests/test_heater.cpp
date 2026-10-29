#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "heater_control.h"
#include "sampling.h"

struct MockHeater : public HeaterControllerBase
{
    MockHeater() : HeaterControllerBase(0, 5, 10) { }

    MOCK_METHOD(void, SetDuty, (float), (const, override));
};

TEST(HeaterStateOutput, Preheat)
{
    MockHeater dut;

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
    dut.Configure(780, 300);

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

    EXPECT_EQ(0, dut.GetVoltageForState(HeaterState::Stopped, 0));
}

TEST(HeaterStateMachine, PreheatToWarmupTimeout)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300);

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
    dut.Configure(780, 300);

    // Preheat for a little while
    for (size_t i = 0; i < 10; i++)
    {
        Timer::advanceMockTime(100 * 1000);
        EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 500));
    }

    // Sensor magically warms up, skip to warmup ramp!
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::Preheat, HeaterAllow::Allowed, 12, 780));
}

TEST(HeaterStateMachine, WarmupToClosedLoop)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300);

    // Skip stabilization timer: 500+ mS
    EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    Timer::advanceMockTime(501 * 1000);

    // Warm up for a little while
    for (size_t i = 0; i < 10; i++)
    {
        Timer::advanceMockTime(100 * 1000);
        EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    }

    // Sensor warms up, time for closed loop!
    EXPECT_EQ(HeaterState::ClosedLoop, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 780));
}

TEST(HeaterStateMachine, WarmupTimeout)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300);

    // For a while it should stay in warmup
    Timer::setMockTime(1e6);
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    Timer::setMockTime(2e6);
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    Timer::setMockTime(9.9e6);
    EXPECT_EQ(HeaterState::WarmupRamp, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));

    // Warmup times out, sensor transitions to stopped
    Timer::setMockTime(10.1e6);
    EXPECT_EQ(HeaterState::Stopped, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
}

TEST(HeaterStateMachine, ClosedLoop)
{
    MockHeater dut;
    Timer::setMockTime(0);
    dut.Configure(780, 300);

    // Skip stabilization timer: 500+ mS
    EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    Timer::advanceMockTime(501 * 1000);

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
    dut.Configure(780, 300);

    // Skip stabilization timer: 500+ mS
    EXPECT_EQ(HeaterState::Preheat, dut.GetNextState(HeaterState::WarmupRamp, HeaterAllow::Allowed, 12, 500));
    Timer::advanceMockTime(501 * 1000);

    EXPECT_EQ(HeaterState::Stopped, dut.GetNextState(HeaterState::Stopped, HeaterAllow::Allowed, 12, 780));
}

struct MockHeaterSampler : public ISampler
{
    MOCK_METHOD(float, GetNernstDc, (), (const, override));
    MOCK_METHOD(float, GetNernstAc, (), (const, override));
    MOCK_METHOD(float, GetPumpNominalCurrent, (), (const, override));
#ifdef HEATER_INPUT_DIVIDER
    MOCK_METHOD(float, GetInternalHeaterVoltage, (), (const, override));
#endif
    MOCK_METHOD(float, GetSensorTemperature, (), (const, override));
    MOCK_METHOD(float, GetSensorInternalResistance, (), (const, override));
};

TEST(HeaterOutput, NegativeVoltageDoesNotBecomePositiveDuty)
{
    using namespace testing;
    Timer::setMockTime(0);
    NiceMock<MockHeater> dut;
    NiceMock<MockHeaterSampler> sampler;
    dut.Configure(730, 80);
    Timer::advanceMockTime(501 * 1000); // Production supply stabilization
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
    dut.Configure(730, 80);
    Timer::advanceMockTime(501 * 1000); // Production supply stabilization
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

TEST(HeaterOutput, LowSupplyDoesNotUseNominalVoltage)
{
    using namespace testing;
    Timer::setMockTime(0);
    NiceMock<MockHeater> dut;
    NiceMock<MockHeaterSampler> sampler;
    dut.Configure(780, 300);
    ON_CALL(sampler, GetSensorTemperature()).WillByDefault(Return(25));
    ON_CALL(sampler, GetSensorInternalResistance()).WillByDefault(Return(10000));

    for (float supply : {0.0f, 0.5f, 0.99f}) {
        testRemoteBatteryVoltage = supply;
        EXPECT_CALL(dut, SetDuty(0)).Times(1);
        dut.Update(sampler, HeaterAllow::Allowed);
        EXPECT_FLOAT_EQ(0, dut.GetHeaterEffectiveVoltage());
        EXPECT_EQ(HeaterState::Preheat, dut.GetHeaterState());
        Mock::VerifyAndClearExpectations(&dut);
    }

    // Production permits 2 V preheat below its warmup supply threshold.
    // Duty must use the actual supply, not the former 12 V substitution.
    testRemoteBatteryVoltage = 2;
    EXPECT_CALL(dut, SetDuty(1)).Times(1);
    dut.Update(sampler, HeaterAllow::Allowed);
    EXPECT_FLOAT_EQ(2, dut.GetHeaterEffectiveVoltage());
    EXPECT_EQ(HeaterState::Preheat, dut.GetHeaterState());
    testRemoteBatteryVoltage = 13.7f;
}

#include <gtest/gtest.h>

#include "sampling.h"
#include "port.h"

SensorType GetSensorType()
{
    return SensorType::LSU49;
}

int GetESRSupplyR()
{
    // Nernst AC injection resistor value
    return 22000;
}

TEST(Sampler, TestDc)
{
    Sampler dut;

    AnalogChannelResult data;
    data.NernstVoltage = 0.45f;
    data.PumpCurrentVoltage = 1.75f;
    data.NernstClamped = false;
    constexpr float virtualGroundVoltage = 1.65f;

    for (size_t i = 0; i < 5000; i++)
    {
        dut.ApplySample(data, virtualGroundVoltage);
    }

    // not exactly 0 because of filtering
    EXPECT_NEAR(0, dut.GetNernstAc(), 1e-3);
    EXPECT_FLOAT_EQ(0.45f, dut.GetNernstDc());
    EXPECT_NEAR(-0.1616, dut.GetPumpNominalCurrent(), 1e-3);
}

TEST(Sampler, TestAc)
{
    Sampler dut;

    AnalogChannelResult dataLow;
    dataLow.NernstVoltage = 0.45f - 0.1f;
    dataLow.PumpCurrentVoltage = 1.75f;
    dataLow.NernstClamped = false;

    AnalogChannelResult dataHigh;
    dataHigh.NernstVoltage = 0.45f + 0.1f;
    dataHigh.PumpCurrentVoltage = 1.75f;
    dataHigh.NernstClamped = false;

    constexpr float virtualGroundVoltage = 1.65f;

    for (size_t i = 0; i < 5000; i++)
    {
        dut.ApplySample(dataLow,  virtualGroundVoltage);
        dut.ApplySample(dataHigh, virtualGroundVoltage);
    }

    EXPECT_NEAR(0.2, dut.GetNernstAc(), 1e-3);
    EXPECT_FLOAT_EQ(0.45f, dut.GetNernstDc());
    EXPECT_NEAR(-0.1616, dut.GetPumpNominalCurrent(), 1e-3);
}

TEST(Sampler, ClampedInputIsNotReportedAsHot)
{
    Sampler dut;
    AnalogChannelResult data{};
    data.NernstClamped = true;

    // A clipped constant signal has no AC component. It must not be
    // interpreted as low ESR and a hot sensor, including at startup.
    for (float voltage : {0.0f, 1.65f}) {
        data.NernstVoltage = voltage;
        for (int i = 0; i < 200; i++) {
            dut.ApplySample(data, 1.65f);
            EXPECT_FLOAT_EQ(10000, dut.GetSensorInternalResistance());
            EXPECT_LT(dut.GetSensorTemperature(), 100);
        }
    }
}

TEST(Sampler, EsrRecoversAfterCleanSamples)
{
    Sampler dut;
    AnalogChannelResult data{};
    for (int i = 0; i < 5000; i++) {
        data.NernstVoltage = (i % 2) ? 0.55f : 0.35f;
        dut.ApplySample(data, 1.65f);
    }

    data.NernstClamped = true;
    dut.ApplySample(data, 1.65f);
    EXPECT_FLOAT_EQ(10000, dut.GetSensorInternalResistance());

    // A new clipped sample restarts the recovery window.
    data.NernstClamped = false;
    for (int i = 0; i < 50; i++) {
        dut.ApplySample(data, 1.65f);
    }
    data.NernstClamped = true;
    dut.ApplySample(data, 1.65f);
    data.NernstClamped = false;
    for (int i = 0; i < 99; i++) {
        data.NernstVoltage = (i % 2) ? 0.55f : 0.35f;
        dut.ApplySample(data, 1.65f);
        EXPECT_FLOAT_EQ(10000, dut.GetSensorInternalResistance());
    }
    data.NernstVoltage = 0.55f;
    dut.ApplySample(data, 1.65f);
    EXPECT_GT(dut.GetSensorInternalResistance(), 0);
    EXPECT_LT(dut.GetSensorInternalResistance(), 2000);
}

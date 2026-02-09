#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstring>
#include "fixed_point.h"
#include "port.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
namespace ConfigSizes {
    constexpr size_t TAG = 4;
    constexpr size_t NO_LONGER_USED_0 = 1;
    constexpr size_t AUX_OUT_BINS = 64;
    constexpr size_t AUX_OUT_VALUES = 64;
    constexpr size_t AUX_OUTPUT_SOURCE = 2;
    constexpr size_t SENSOR_TYPE = 1;
    constexpr size_t AFR_CHANNEL = 8;
    constexpr size_t AFR_SETTINGS = AFR_CHANNEL * 2;
    constexpr size_t EGT_CHANNEL = 8;
    constexpr size_t EGT_SETTINGS = EGT_CHANNEL * 2;
    constexpr size_t HEATER_CONFIG = 8;
}
#pragma GCC diagnostic pop

template<typename T>
void WriteAtOffset(Configuration& config, size_t offset, T value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&config);
    std::memcpy(bytes + offset, &value, sizeof(T));
}

TEST(ConfigLayout, BinaryCompatibility_NoLongerUsed0_ZeroInit) {
    Configuration config = {};
    
    EXPECT_EQ(config.NoLongerUsed0, 0);
}

TEST(ConfigLayout, BinaryCompatibility_NoLongerUsed0) {
    Configuration config = {};
    
    size_t offset = ConfigSizes::TAG;
    uint8_t expectedValue = 0x26;
    WriteAtOffset(config, offset, expectedValue);
    
    EXPECT_EQ(config.NoLongerUsed0, expectedValue);
}

TEST(ConfigLayout, BinaryCompatibility_AuxOutBins) {
    Configuration config = {};

    size_t offset = ConfigSizes::TAG
                  + ConfigSizes::NO_LONGER_USED_0;
    
    // Test auxOutBins[0]
    for (int i = 0; i < 8; i++) {
        float testValue = 10.0f + i;
        WriteAtOffset(config, offset + i * sizeof(float), testValue);
    }
    
    for (int i = 0; i < 8; i++) {
        EXPECT_FLOAT_EQ(config.auxOutBins[0][i], 10.0f + i);
    }

    // Test auxOutBins[1]
    offset += 8 * sizeof(float);
    for (int i = 0; i < 8; i++) {
        float testValue = 20.0f + i;
        WriteAtOffset(config, offset + i * sizeof(float), testValue);
    }

    for (int i = 0; i < 8; i++) {
        EXPECT_FLOAT_EQ(config.auxOutBins[1][i], 20.0f + i);
    }
}

TEST(ConfigLayout, BinaryCompatibility_AuxOutValues) {
    Configuration config = {};

    size_t offset = ConfigSizes::TAG
                  + ConfigSizes::NO_LONGER_USED_0
                  + ConfigSizes::AUX_OUT_BINS;

    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 8; i++) {
            float testValue = 100.0f + j * 10 + i;
            WriteAtOffset(config, offset + (j * 8 + i) * sizeof(float), testValue);
        }
    }

    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 8; i++) {
            EXPECT_FLOAT_EQ(config.auxOutValues[j][i], 100.0f + j * 10 + i);
        }
    }
}

TEST(ConfigLayout, BinaryCompatibility_AuxOutputSource) {
    Configuration config = {};

    size_t offset = ConfigSizes::TAG
                  + ConfigSizes::NO_LONGER_USED_0
                  + ConfigSizes::AUX_OUT_BINS
                  + ConfigSizes::AUX_OUT_VALUES;
    
    WriteAtOffset(config, offset, static_cast<uint8_t>(AuxOutputMode::Lambda0));
    WriteAtOffset(config, offset + 1, static_cast<uint8_t>(AuxOutputMode::Egt1));
    
    EXPECT_EQ(config.auxOutputSource[0], AuxOutputMode::Lambda0);
    EXPECT_EQ(config.auxOutputSource[1], AuxOutputMode::Egt1);
}

TEST(ConfigLayout, BinaryCompatibility_SensorType) {
    Configuration config = {};
    
    size_t offset = ConfigSizes::TAG
                  + ConfigSizes::NO_LONGER_USED_0
                  + ConfigSizes::AUX_OUT_BINS
                  + ConfigSizes::AUX_OUT_VALUES
                  + ConfigSizes::AUX_OUTPUT_SOURCE;
    
    WriteAtOffset(config, offset, static_cast<uint8_t>(SensorType::LSU42));
    
    EXPECT_EQ(config.sensorType, SensorType::LSU42);
}

TEST(ConfigLayout, BinaryCompatibility_AfrChannelSettings) {
    Configuration config = {};
    
    size_t offset = ConfigSizes::TAG
                  + ConfigSizes::NO_LONGER_USED_0
                  + ConfigSizes::AUX_OUT_BINS
                  + ConfigSizes::AUX_OUT_VALUES
                  + ConfigSizes::AUX_OUTPUT_SOURCE
                  + ConfigSizes::SENSOR_TYPE;
    
    // Write first AFR channel
    uint8_t bitfield0 = 0b00000111; // RusEfiTx=1, RusEfiTxDiag=1, ExtraCanProtocol=1
    WriteAtOffset(config, offset, bitfield0);
    WriteAtOffset(config, offset + 1, static_cast<uint8_t>(5)); // RusEfiIdx
    WriteAtOffset(config, offset + 2, static_cast<uint8_t>(10)); // ExtraCanIdOffset
    
    EXPECT_TRUE(config.afr[0].RusEfiTx);
    EXPECT_TRUE(config.afr[0].RusEfiTxDiag);
    EXPECT_TRUE(config.afr[0].ExtraCanProtocol == CanProtocol::AemNet);
    EXPECT_EQ(config.afr[0].RusEfiIdx, 5);
    EXPECT_EQ(config.afr[0].ExtraCanIdOffset, 10);
    
    // Write second AFR channel
    offset += ConfigSizes::AFR_CHANNEL;
    uint8_t bitfield1 = 0b00000010; // RusEfiTx=0, RusEfiTxDiag=1, ExtraCanProtocol=0
    WriteAtOffset(config, offset, bitfield1);
    WriteAtOffset(config, offset + 1, static_cast<uint8_t>(7)); // RusEfiIdx
    WriteAtOffset(config, offset + 2, static_cast<uint8_t>(15)); // ExtraCanIdOffset
    
    EXPECT_FALSE(config.afr[1].RusEfiTx);
    EXPECT_TRUE(config.afr[1].RusEfiTxDiag);
    EXPECT_TRUE(config.afr[1].ExtraCanProtocol == CanProtocol::None);
    EXPECT_EQ(config.afr[1].RusEfiIdx, 7);
    EXPECT_EQ(config.afr[1].ExtraCanIdOffset, 15);
}

TEST(ConfigLayout, BinaryCompatibility_EgtChannelSettings) {
    Configuration config = {};

    size_t offset = ConfigSizes::TAG
                  + ConfigSizes::NO_LONGER_USED_0
                  + ConfigSizes::AUX_OUT_BINS
                  + ConfigSizes::AUX_OUT_VALUES
                  + ConfigSizes::AUX_OUTPUT_SOURCE
                  + ConfigSizes::SENSOR_TYPE
                  + ConfigSizes::AFR_SETTINGS;
    
    // Write first EGT channel
    uint8_t bitfield0 = 0b00000101; // RusEfiTx=1, RusEfiTxDiag=0, ExtraCanProtocol=AemNet
    WriteAtOffset(config, offset, bitfield0);
    WriteAtOffset(config, offset + 1, static_cast<uint8_t>(3)); // RusEfiIdx
    WriteAtOffset(config, offset + 2, static_cast<uint8_t>(8)); // ExtraCanIdOffset
    
    EXPECT_TRUE(config.egt[0].RusEfiTx);
    EXPECT_FALSE(config.egt[0].RusEfiTxDiag);
    EXPECT_TRUE(config.egt[0].ExtraCanProtocol == CanProtocol::AemNet);
    EXPECT_EQ(config.egt[0].RusEfiIdx, 3);
    EXPECT_EQ(config.egt[0].ExtraCanIdOffset, 8);

    // Write second EGT channel
    offset += ConfigSizes::EGT_CHANNEL;
    uint8_t bitfield1 = 0b00000010; // RusEfiTx=0, RusEfiTxDiag=1, ExtraCanProtocol=0
    WriteAtOffset(config, offset, bitfield1);
    WriteAtOffset(config, offset + 1, static_cast<uint8_t>(7)); // RusEfiIdx
    WriteAtOffset(config, offset + 2, static_cast<uint8_t>(15)); // ExtraCanIdOffset
    
    EXPECT_FALSE(config.egt[1].RusEfiTx);
    EXPECT_TRUE(config.egt[1].RusEfiTxDiag);
    EXPECT_TRUE(config.egt[1].ExtraCanProtocol == CanProtocol::None);
    EXPECT_EQ(config.egt[1].RusEfiIdx, 7);
    EXPECT_EQ(config.egt[1].ExtraCanIdOffset, 15);
}

TEST(ConfigLayout, BinaryCompatibility_HeaterConfig) {
    Configuration config = {};

    size_t offset = ConfigSizes::TAG
                  + ConfigSizes::NO_LONGER_USED_0
                  + ConfigSizes::AUX_OUT_BINS
                  + ConfigSizes::AUX_OUT_VALUES
                  + ConfigSizes::AUX_OUTPUT_SOURCE
                  + ConfigSizes::SENSOR_TYPE
                  + ConfigSizes::AFR_SETTINGS
                  + ConfigSizes::EGT_SETTINGS;
    
    WriteAtOffset(config, offset++, static_cast<uint8_t>(120)); // HeaterSupplyOffVoltage
    WriteAtOffset(config, offset++, static_cast<uint8_t>(135)); // HeaterSupplyOnVoltage
    WriteAtOffset(config, offset++, static_cast<uint8_t>(25)); // PreheatTimeSec
    
    EXPECT_FLOAT_EQ(config.heaterConfig.HeaterSupplyOffVoltage.getValue(), 12.0f);
    EXPECT_FLOAT_EQ(config.heaterConfig.HeaterSupplyOnVoltage.getValue(), 13.5f);
    EXPECT_FLOAT_EQ(config.heaterConfig.PreheatTimeSec, 125.0f);
}

TEST(ConfigLayout, SizeVerification) {
    // Verify the total size is exactly 256 bytes
    EXPECT_EQ(sizeof(Configuration), 256UL);
    
    // Verify union size
    Configuration config;
    EXPECT_EQ(sizeof(config.pad), 252UL); // 256 - 4 (Tag size)
}
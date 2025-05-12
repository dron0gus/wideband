#pragma once

#include <cstdint>
#include <cstddef>

#include "port_shared.h"
#include "wideband_config.h"

struct AnalogChannelResult
{
    float NernstVoltage;
    float PumpCurrentVoltage;
    /* for dual version - this is voltage on Heater-, switches between zero and Vbatt with heater PWM,
        * used for both Vbatt measurement and Heater diagnostic */
    float HeaterSupplyVoltage;
    /* If measured voltage is too close to ground or Vref assume value is clamped */
    bool NernstClamped;
};

struct AnalogResult
{
    AnalogChannelResult ch[AFR_CHANNELS];
    float VirtualGroundVoltageInt;

    #ifdef BOARD_HAS_VOLTAGE_SENSE
    float SupplyVoltage;
    #endif

    float McuTemp;
};

// Enable ADCs, configure pins, etc
void PortPrepareAnalogSampling();
void AnalogSampleStart();
AnalogResult AnalogSampleFinish();

enum class SensorType : uint8_t {
    LSU49 = 0,
    LSU42 = 1,
    LSUADV = 2,
};

#ifndef BOARD_DEFAULT_SENSOR_TYPE
#define BOARD_DEFAULT_SENSOR_TYPE SensorType::LSU49
#endif

enum class AuxOutputMode : uint8_t {
    Afr0 = 0,
    Afr1 = 1,
    Lambda0 = 2,
    Lambda1 = 3,
    Egt0 = 4,
    Egt1 = 5,
};

class Configuration {
private:
    // Increment this any time the configuration format changes
    // It is stored along with the data to ensure that it has been written before
    static constexpr uint32_t ExpectedTag = 0xDEADBE02;
    uint32_t Tag = ExpectedTag;

public:
    bool IsValid() const
    {
        return this->Tag == ExpectedTag;
    }

    // Configuration defaults
    void LoadDefaults()
    {
        int i;

        *this = {};

        NoLongerUsed0 = 0;
        sensorType = BOARD_DEFAULT_SENSOR_TYPE;

        /* default auxout curve is 0..5V for AFR 8.5 to 18.0
         * default auxout[n] input is AFR[n] */
        for (i = 0; i < 8; i++) {
            auxOutBins[0][i] = auxOutBins[1][i] = 8.5 + (18.0 - 8.5) / 7 * i;
            auxOutValues[0][i] = auxOutValues[1][i] = 0.0 + (5.0 - 0.0) / 7 * i;
        }
        auxOutputSource[0] = AuxOutputMode::Afr0;
        auxOutputSource[1] = AuxOutputMode::Afr1;

        for (i = 0; i < AFR_CHANNELS; i++) {
            // enable RusEFI protocol
            afr[i].RusEfiTx = true;
            afr[i].RusEfiTxDiag = true;
            afr[i].RusEfiIdOffset = 2 * i;

            // Disable AemNet
            afr[i].AemNetTx = false;
            afr[i].AemNetIdOffset = i;
        }

        for (i = 0; i < EGT_CHANNELS; i++) {
            // disable RusEFI protocol - not implemented
            egt[i].RusEfiTx = false;
            egt[i].RusEfiTxDiag = false;
            egt[i].RusEfiIdOffset = i;

            // Enable AemNet
            egt[i].AemNetTx = true;
            egt[i].AemNetIdOffset = i;
        }

        /* Finaly */
        Tag = ExpectedTag;
    }

    // Actual configuration data
    union {
        struct {
            uint8_t NoLongerUsed0 = 0;
            // AUX0 and AUX1 curves
            float auxOutBins[2][8];
            float auxOutValues[2][8];
            AuxOutputMode auxOutputSource[2];

            SensorType sensorType;

            // per AFR channel settings
            struct {
                bool RusEfiTx:1;
                bool RusEfiTxDiag:1;
                bool AemNetTx:1;

                uint8_t RusEfiIdOffset;
                uint8_t AemNetIdOffset;
                uint8_t pad[5];
            } afr[2];

            // per EGT channel settings
            struct {
                bool RusEfiTx:1;
                bool RusEfiTxDiag:1;
                bool AemNetTx:1;

                uint8_t RusEfiIdOffset;
                uint8_t AemNetIdOffset;
                uint8_t pad[5];
            } egt[2];
        } __attribute__((packed));

        // pad to 256 bytes including tag
        uint8_t pad[256 - sizeof(Tag)];
    };
};

int InitConfiguration();
Configuration* GetConfiguration();
void SetConfiguration();

/* TS stuff */
uint8_t *GetConfigurationPtr();
size_t GetConfigurationSize();
int SaveConfiguration();
const char *getTsSignature();

void rebootNow();
void rebootToOpenblt();
void rebootToDfu();

extern "C" void checkDfuAndJump();

// LSU4.2, LSU4.9 or LSU_ADV
SensorType GetSensorType();
void SetupESRDriver(SensorType sensor);
void ToggleESRDriver(SensorType sensor);
int GetESRSupplyR();

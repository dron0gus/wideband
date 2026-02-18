#pragma once

#if WB_PROD

#include "hal.h"

const CANConfig& GetCanConfig(uint8_t mode);

#endif // WB_PROD

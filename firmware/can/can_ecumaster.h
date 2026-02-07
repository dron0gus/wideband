#pragma once

#include <cstdint>

#include "hal.h"
#include "port.h"

void SendEcuMasterAfrFormat(Configuration* configuration, uint8_t ch);

void SendEcuMasterEgtFormat(Configuration* configuration, uint8_t ch);
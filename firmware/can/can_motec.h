#pragma once

#include <cstdint>

#include "hal.h"
#include "port.h"

void SendMotecAfrFormat(Configuration* configuration, uint8_t ch);
void SendMotecEgtFormat(Configuration* configuration, uint8_t ch);
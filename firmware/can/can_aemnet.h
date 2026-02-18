#pragma once

#include <cstdint>

#include "hal.h"
#include "port.h"

void SendAemNetUEGOFormat(Configuration* cfg, uint8_t ch);

void SendAemNetEGT0305Format(Configuration* cfg);
void SendAemNetEGT2224Format(Configuration* cfg);
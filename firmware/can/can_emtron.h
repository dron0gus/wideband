#pragma once

#include <cstdint>

#include "hal.h"
#include "port.h"

void SendEmtronAfrFormat(Configuration* configuration, uint8_t ch);
void SendEmtronEgtFormat(Configuration* configuration, uint8_t ch);

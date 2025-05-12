#pragma once

#include <cstdint>
#include "byteswap.h"
#include "port.h"

void SendAemNetUEGOFormat(Configuration* cfg, uint8_t ch);
void SendAemNetEGTFormat(Configuration* cfg, uint8_t ch);

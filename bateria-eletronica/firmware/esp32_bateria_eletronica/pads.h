#ifndef PADS_H
#define PADS_H

#include <Arduino.h>
#include "config.h"

enum HiHatZone { HIHAT_CLOSED, HIHAT_HALF, HIHAT_OPEN };

void padsInit();
void padsTask(void *param);

#endif

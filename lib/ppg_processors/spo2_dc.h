#ifndef SPO2_DC_H
#define SPO2_DC_H

#include <stdint.h>
#include "processor_types.h"

void updateSpo2Dc(spo2 &spo2Data, int &spo2_idx, uint32_t ir, uint32_t red);

#endif
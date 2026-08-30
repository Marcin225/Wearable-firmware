#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H

#include <stdint.h>
#include "mpu6050_driver.h"

int32_t calculateMotion(const MpuSample *mpuBatch, int mpuCount);
MpuSample interpolateMpu(const MpuSample *mpuBatch, int mpuCount, int sampleNumber, int maxCount);

#endif
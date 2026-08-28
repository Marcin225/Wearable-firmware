#include "sensor_processing.h"
#include "common.h"

int32_t calculateMotion(const MpuSample *mpuBatch, int mpuCount) {

    int32_t max_motion_X = INT32_MIN, max_motion_Y = INT32_MIN, max_motion_Z = INT32_MIN;
    int32_t min_motion_X = INT32_MAX, min_motion_Y = INT32_MAX, min_motion_Z = INT32_MAX;

    for (int s = 0; s < mpuCount; s++) {
        if (mpuBatch[s].accX > max_motion_X) max_motion_X = mpuBatch[s].accX;
        if (mpuBatch[s].accY > max_motion_Y) max_motion_Y = mpuBatch[s].accY;
        if (mpuBatch[s].accZ > max_motion_Z) max_motion_Z = mpuBatch[s].accZ;

        if (mpuBatch[s].accX < min_motion_X) min_motion_X = mpuBatch[s].accX;
        if (mpuBatch[s].accY < min_motion_Y) min_motion_Y = mpuBatch[s].accY;
        if (mpuBatch[s].accZ < min_motion_Z) min_motion_Z = mpuBatch[s].accZ;
    }

    int32_t diffX = absValueOf(max_motion_X - min_motion_X);
    int32_t diffY = absValueOf(max_motion_Y - min_motion_Y);
    int32_t diffZ = absValueOf(max_motion_Z - min_motion_Z);

    int32_t motion = diffX + diffY + diffZ;

    return motion;
}

MpuSample interpolateMpu(const MpuSample *mpuBatch, int mpuCount, int sampleNumber, int maxCount) {
    MpuSample output = {0};

    if (mpuCount <= 0) {
        return output;
    }

    if (mpuCount == 1 || maxCount <= 1) {
        return mpuBatch[0];
    }

    if (sampleNumber >= maxCount - 1) {
        return mpuBatch[mpuCount - 1];
    }

    int posMpuSample = sampleNumber * (mpuCount - 1);
    int length = maxCount - 1;

    int Idx = posMpuSample / length;

    int num = posMpuSample % length; // how far is mpu sample beetwen max (sensor) samples 
    int den = length; // number of samples

    output.accX = mpuBatch[Idx].accX + ((int64_t)(mpuBatch[Idx + 1].accX - mpuBatch[Idx].accX) * num) / den;
    output.accY = mpuBatch[Idx].accY + ((int64_t)(mpuBatch[Idx + 1].accY - mpuBatch[Idx].accY) * num) / den;
    output.accZ = mpuBatch[Idx].accZ + ((int64_t)(mpuBatch[Idx + 1].accZ - mpuBatch[Idx].accZ) * num) / den;

    return output;
}
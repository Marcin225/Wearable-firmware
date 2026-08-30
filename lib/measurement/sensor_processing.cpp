#include "sensor_processing.h"
#include "common.h"


// calculate motion as the sum of peak-to-peak acceleration across all three axes
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

// linearly interpolate MPU6050 samples to match the MAX30102 sample count
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

    int idx = posMpuSample / length;

    int num = posMpuSample % length;
    int den = length;

    output.accX = mpuBatch[idx].accX + ((int64_t)(mpuBatch[idx + 1].accX - mpuBatch[idx].accX) * num) / den;
    output.accY = mpuBatch[idx].accY + ((int64_t)(mpuBatch[idx + 1].accY - mpuBatch[idx].accY) * num) / den;
    output.accZ = mpuBatch[idx].accZ + ((int64_t)(mpuBatch[idx + 1].accZ - mpuBatch[idx].accZ) * num) / den;

    return output;
}
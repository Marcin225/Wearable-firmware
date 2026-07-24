#ifndef PPG_PROCESSOR_H
#define PPG_PROCESSOR_H

#include <stdint.h>

class SignalProcessingAlgorithms {
    public:
        SignalProcessingAlgorithms(void);

        int32_t process_fft();
};


#endif
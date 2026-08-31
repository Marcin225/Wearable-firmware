#ifndef CONFIG_H
#define CONFIG_H

// global system configuration parameters

#define BUFFER_SIZE                 1024 // number of samples in the full processing window
#define CHUNK_SIZE                  256 // number of samples passed between tasks at once
#define SPECTRUM_SIZE               1025 // number of bins in the positive-frequency FFT spectrum
#define FFT_SIZE                    2048 // FFT size including zero padding

#define I2C_SDA_PIN                 6
#define I2C_SCL_PIN                 7

#define SAMPLING_RATE_HZ            100 // PPG sampling rate in Hz
#define FINGER_IR_THRESHOLD         50000 // minimum IR level used to detect finger contact
#define MOTION_THRESHOLD            40000 // accelerometer motion detection threshold

#define TASK_DATA_STACK_SIZE        4096 // collector task stack size in bytes
#define TASK_CALC_STACK_SIZE        8192 // calculation task stack size in bytes
#define TASK_DATA_PRIORITY          2 // collector task priority
#define TASK_CALC_PRIORITY          1 // calculation task priority

#define BONUS_Q12                   61 // HR candidate continuity bonus weight in Q12
#define MAIN_PENALTY_Q12            4506 // motion penalty weight in Q12
#define TH_CF_Q12                   29491 // minimum peak-to-mean power ratio for a valid HR peak in Q12

#endif
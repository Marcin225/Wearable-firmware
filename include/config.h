#ifndef CONFIG_H
#define CONFIG_H

#define I2C_SDA_PIN                 6
#define I2C_SCL_PIN                 7

#define SAMPLING_RATE_HZ            100
#define FINGER_IR_THRESHOLD         50000

#define TASK_DATA_STACK_SIZE        2048
#define TASK_CALC_STACK_SIZE        4096
#define TASK_DATA_PRIORITY          2
#define TASK_CALC_PRIORITY          1

#define NLMS_NUM_OF_TAPS            32
#define NLMS_MU                     512
#define NLMS_EPS                    1

#endif
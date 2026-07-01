#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "SystemContext.h"
#include "tasks.h"

SystemContext sysContext;

void setup() {
  delay(4000);
  Serial.begin(115200);

  // Init I2C, sensors and tasks with queues

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(300);

  if (!sysContext.maxSensor.begin()) {
    Serial.println("Max30102 Not Found / Init Error");
  } else {
    Serial.println("Max30102 Ok");
  }
  delay(20);

  if (!sysContext.mpuSensor.begin()) {
    Serial.println("Mpu6050 Not Found / Init Error");
  }else {
    Serial.println("Mpu6050 Ok");
  }

  if (!sysContext.BLE.begin()) {
    Serial.println("BLE Init Error");
  }else {
    Serial.println("BLE Ok");
  }

  // Queue for pulse & motion data between tasks

  sysContext.emptyQueue = xQueueCreate(2, sizeof(PulseData *));
  sysContext.fullQueue = xQueueCreate(2, sizeof(PulseData *));

  if (sysContext.emptyQueue == NULL || sysContext.fullQueue == NULL) {
    Serial.println("Queue creation failed");
    for (;;) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  PulseData *ptrQueue1 = &sysContext.pulseBufferA;
  PulseData *ptrQueue2 = &sysContext.pulseBufferB;
  
  if (xQueueSend(sysContext.emptyQueue, &ptrQueue1, portMAX_DELAY) != pdTRUE) {
    Serial.println("Failed to seed emptyQueue with buffer A");
  }

  if (xQueueSend(sysContext.emptyQueue, &ptrQueue2, portMAX_DELAY) != pdTRUE) {
    Serial.println("Failed to seed emptyQueue with buffer B");
  }

  xTaskCreate(vCollectAndFilterDataTask, "dataCollectorTask", TASK_DATA_STACK_SIZE, &sysContext, TASK_DATA_PRIORITY, NULL); //1536
  xTaskCreate(vCalculateVitalsTask, "vitalsCalculationTask", TASK_CALC_STACK_SIZE, &sysContext, TASK_CALC_PRIORITY, NULL); // 1024
}

void loop() {
  // not used, RTOS tasks manage all code execution
}

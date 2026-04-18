#include <Arduino.h>
#include <Wire.h>
#include "tasks.h"

void setup() {
  delay(4000);
  Serial.begin(921600);

  Wire.begin(6, 7);
  delay(300);

  if (!maxSensor.begin()) {
    Serial.println("Max30102 Not Found / Init Error");
  } else {
    Serial.println("Max30102 Ok");
  }

  if (!mpuSensor.begin()) {
    Serial.println("Mpu6050 Not Found / Init Error");
  }else {
    Serial.println("Mpu6050 Ok");
  }
  // Queue for pulse & motion data between tasks
  emptyQueue = xQueueCreate(2, sizeof(PulseData *));
  fullQueue = xQueueCreate(2, sizeof(PulseData *));

  if (emptyQueue == NULL || fullQueue == NULL) {
    Serial.println("Queue creation failed");
    for (;;) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  PulseData *ptrQueue1 = &pulseBufferA;
  PulseData *ptrQueue2 = &pulseBufferB;
  
  if (xQueueSend(emptyQueue, &ptrQueue1, portMAX_DELAY) != pdTRUE) {
    Serial.println("Failed to seed emptyQueue with buffer A");
  }

  if (xQueueSend(emptyQueue, &ptrQueue2, portMAX_DELAY) != pdTRUE) {
    Serial.println("Failed to seed emptyQueue with buffer B");
  }

  xTaskCreate(vCollectAndFilterDataTask, "dataCollectorTask", 2048, NULL, 2, NULL); //1536
  xTaskCreate(vCalculateVitalsTask, "vitalsCalculationTask", 4096, NULL, 1, NULL); // 1024
}

void loop() {

}

#include <Arduino.h>
#include <Wire.h>
#include "tasks.h"

void setup() {
  delay(4000);
  Serial.begin(115200);

  Wire.begin(6, 7);
  delay(300);

  if (!maxSensor.begin()) {
    Serial.println("Max30102 Not Found / Init Error");
  } else {
    Serial.println("Max30102 Ok");
  }

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

  xTaskCreate(vCollectAndFilterDataTask, "dataCollectorTask", 1536, NULL, 2, NULL);
  xTaskCreate(vCalculateVitalsTask, "vitalsCalculationTask", 1024, NULL, 1, NULL);
}

void loop() {

}

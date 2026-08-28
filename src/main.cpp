#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "SystemContext.h"
#include "tasks.h"

SystemContext sysContext;

void setup() {
  delay(4000);
  Serial.begin(921600);

  // Init I2C, sensors and tasks with queues

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(300);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
    system_mode = DeviceState::CHECK;
  }

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

  delay(20);

  // if (!sysContext.BLE.begin()) {
  //   Serial.println("BLE Init Error");
  // }else {
  //   Serial.println("BLE Ok");
  // }

  // Queue for pulse & motion data between tasks

  sysContext.emptyQueue = xQueueCreate(2, sizeof(pulseData *));
  sysContext.fullQueue = xQueueCreate(2, sizeof(pulseData *));

  if (sysContext.emptyQueue == NULL || sysContext.fullQueue == NULL) {
    Serial.println("Queue creation failed");
    for (;;) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  pulseData *ptrQueue1 = &sysContext.pulseBufferA;
  pulseData *ptrQueue2 = &sysContext.pulseBufferB;
  
  if (xQueueSend(sysContext.emptyQueue, &ptrQueue1, portMAX_DELAY) != pdTRUE) {
    Serial.println("Failed to seed emptyQueue with buffer A");
  }

  if (xQueueSend(sysContext.emptyQueue, &ptrQueue2, portMAX_DELAY) != pdTRUE) {
    Serial.println("Failed to seed emptyQueue with buffer B");
  }

  xTaskCreate(vCollectAndFilterDataTask, "dataCollectorTask", TASK_DATA_STACK_SIZE, &sysContext, TASK_DATA_PRIORITY, &CollectAndFilterTaskHandle);
  xTaskCreate(vCalculateVitalsTask, "vitalsCalculationTask", TASK_CALC_STACK_SIZE, &sysContext, TASK_CALC_PRIORITY, NULL);

  pinMode(3, INPUT_PULLUP); // init gpio3
  attachInterrupt(3, max30102ISR, FALLING);

  pinMode(2, INPUT_PULLUP);
  esp_deep_sleep_enable_gpio_wakeup((1ULL << 2), ESP_GPIO_WAKEUP_GPIO_LOW);
}

void loop() {
  vTaskDelete(NULL);
  // not used, RTOS tasks manage all code execution
}


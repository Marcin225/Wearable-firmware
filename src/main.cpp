#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "SystemContext.h"
#include "tasks.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_bt.h"

SystemContext sysContext;

// configure dynamic frequency scaling and automatic Light Sleep
void enableAutomaticLightSleep() {
    esp_pm_config_esp32c3_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = true
    };

    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

    Serial.println("Light sleep enabled");
}


void setup() {
  Serial.begin(921600);
  // delay(5000);

  // check if the ESP32-C3 woke up from Deep Sleep using GPIO
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
    sysContext.systemMode = DeviceState::CHECK;
  }

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

  delay(10);

  if (!sysContext.batterySensor.begin()) {
    Serial.println("Max17048 Not Found / Init Error");
  }else {
    Serial.println("Max17048 Ok");
  }

  delay(10);

 // queues pass measurement buffers between the collector and calculation tasks

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

  // MAX30102 INT is active-low and wakes the collector task when FIFO data is ready
  pinMode(MAX30102_INT_PIN, INPUT_PULLUP);
  attachInterrupt(MAX30102_INT_PIN, max30102ISR, ONLOW_WE);

  // keep MAX30102 interrupt GPIO active during Light Sleep
  ESP_ERROR_CHECK(gpio_sleep_sel_dis((gpio_num_t)MAX30102_INT_PIN));

  // allow GPIO interrupts to wake the ESP32-C3 from Light Sleep
  ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

  // MPU motion interrupt wakes the ESP32-C3 from deep sleep
  // the interrupt is active-low
  pinMode(MPU_INT_PIN, INPUT_PULLUP);
  ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup((1ULL << MPU_INT_PIN), ESP_GPIO_WAKEUP_GPIO_LOW));

  enableAutomaticLightSleep();

  if (!sysContext.BLE.begin()) {
    Serial.println("BLE Init Error");
  }else {
    Serial.println("BLE Ok");

    ESP_ERROR_CHECK(esp_bt_sleep_enable());
  }
}

void loop() {
  // FreeRTOS tasks handle the application logic
  vTaskDelete(NULL);
}


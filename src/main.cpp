// #include <Arduino.h>
// #include "max30102_driver.h"
// #include <Wire.h>

// MAX30102 maxSensor;

// void setup() {
//   delay(5000);
//   Serial.begin(115200);
//   Wire.begin(6, 7);
//   delay(1000);
//   if (!maxSensor.begin()) {
//     Serial.println("Max30102 Not Found / Init Error");
//   }else {
//     Serial.println("Max30102 Ok");
//   };

// }

// void loop() {
//   maxSensor.readNewData();
//   while (maxSensor.available()) {
//     MaxSample rawMaxOutput = maxSensor.readSample();
//     Serial.print("IR"); Serial.println(rawMaxOutput.Ir);
//     Serial.print("Red"); Serial.println(rawMaxOutput.Red);
//     Serial.print("\n");
//   }

//   delay(100);
// }


#include <Arduino.h>
#include <Wire.h>

#include "max30102_driver.h"
#include "algorithm_HR.h"

MAX30102 maxSensor;

FilterAlgorithms filters;
SignalProcessingAlgorithms processor;
PulseData pulseBuffer;

int32_t medRed[2] = {0, 0};
int32_t medIr[2]  = {0, 0};

int32_t hpRedState = 0;
int32_t hpIrState  = 0;

int32_t lpRedOut = 0;
int32_t lpIrOut  = 0;

int bufferIdx = 0;

int64_t sumRed = 0;
int64_t sumIr  = 0;

unsigned long lastSampleTime = 0;
const int SAMPLING_RATE = 100;
const int SAMPLE_INTERVAL_MS = 1000 / SAMPLING_RATE;

const int32_t FINGER_IR_THRESHOLD = 50000;

static inline void resetSignalState() {
  bufferIdx = 0;

  sumRed = 0;
  sumIr  = 0;

  hpRedState = 0; hpIrState = 0;
  lpRedOut   = 0; lpIrOut   = 0;

  medRed[0] = 0; medRed[1] = 0;
  medIr[0]  = 0; medIr[1]  = 0;

  for (int i = 0; i < BUFFER_SIZE; i++) {
    pulseBuffer.acRed[i] = 0;
    pulseBuffer.acIr[i]  = 0;
  }
  pulseBuffer.dcRed = 0;
  pulseBuffer.dcIr  = 0;
}


int hrHist[5] = {0,0,0,0,0};
int hrIdx = 0;

static int median5(int *a) {
  int b[5];
  for(int i=0;i<5;i++) b[i]=a[i];
  // prosty sort
  for(int i=0;i<5;i++){
    for(int j=i+1;j<5;j++){
      if(b[j]<b[i]){ int t=b[i]; b[i]=b[j]; b[j]=t; }
    }
  }
  return b[2];
}

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

  resetSignalState();
}

void loop() {
  if (millis() - lastSampleTime < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleTime = millis();

  maxSensor.readNewData();

  while (maxSensor.available()) {
    MaxSample rawMaxOutput = maxSensor.readSample();

    int32_t rawRed = (int32_t)rawMaxOutput.Red;
    int32_t rawIr  = (int32_t)rawMaxOutput.Ir;

    if (rawIr < FINGER_IR_THRESHOLD) {
      resetSignalState();
      return;
    }

    int32_t cleanRed = filters.medianFilter(rawRed, medRed);
    int32_t cleanIr  = filters.medianFilter(rawIr,  medIr);

    medRed[1] = medRed[0]; medRed[0] = rawRed;
    medIr[1]  = medIr[0];  medIr[0]  = rawIr;

    sumRed += cleanRed;
    sumIr  += cleanIr;

    int32_t acRed = filters.highPassFilter(cleanRed, hpRedState);
    int32_t acIr  = filters.highPassFilter(cleanIr,  hpIrState);

    acRed = filters.lowPassFilter(acRed, lpRedOut);
    acIr  = filters.lowPassFilter(acIr,  lpIrOut);

    pulseBuffer.acRed[bufferIdx] = acRed;
    pulseBuffer.acIr[bufferIdx]  = acIr;

    bufferIdx++;

    if (bufferIdx >= BUFFER_SIZE) {
      bufferIdx = 0;

      pulseBuffer.dcRed = (int32_t)(sumRed / BUFFER_SIZE);
      pulseBuffer.dcIr  = (int32_t)(sumIr  / BUFFER_SIZE);
      sumRed = 0;
      sumIr  = 0;

      // Serial.print("AC_RED: ");
      // Serial.println(acRed);
      // Serial.print("AC_IR: ");
      // Serial.println(acIr);
      // Serial.print("DC_RED");
      // Serial.print(pulseBuffer.dcRed);
      // Serial.print("DC_IR");
      // Serial.print(pulseBuffer.dcIr);

      int currentHR = 0;
      int32_t currentSpO2 = processor.calculateSpO2(pulseBuffer, SAMPLING_RATE, currentHR);

      if (currentHR > 0 && currentSpO2 > 0) {
        hrHist[hrIdx] = currentHR;
        hrIdx = (hrIdx + 1) % 5;

        int hrStable = median5(hrHist);
        if (hrStable > 0) {
          Serial.print("HR: "); Serial.print(hrStable);
          Serial.print(" BPM | SpO2: "); Serial.print(currentSpO2);
          Serial.println(" %");
        } else {
          Serial.println("Calculating...");
        }
      } else {
        Serial.println("Calculating... or Finger not detected correctly.");
      }
    }
  }
}

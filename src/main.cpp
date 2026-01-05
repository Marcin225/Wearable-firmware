#include <Arduino.h>


void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.println("Hello, Smart Band!");
}

void loop() {
  Serial.println("Tick");
  delay(1000);
}

#include <ESP32Servo.h>

Servo servoSG90;

#define SERVO_PIN 18

void setup() {
  Serial.begin(115200);

  servoSG90.setPeriodHertz(50);
  servoSG90.attach(SERVO_PIN, 500, 2400);

  Serial.println("SERVO 360 TEST START");
}

void loop() {
  // Quay theo một chiều
  servoSG90.write(0);


}
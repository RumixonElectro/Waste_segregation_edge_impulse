#include <ESP32Servo.h>

Servo myServo;
int servoPin = 2;       // Safe GPIO pin for ESP32-CAM
int currentAngle = 80;   // Keeps track of the current servo position

// LOWER this number for faster speed, INCREASE it for slower speed
const int speedDelay = 1; // Milliseconds to wait between each 1-degree step

void setup() {
  Serial.begin(115200);
  myServo.attach(servoPin);
  
  // Initialize the servo to 0 degrees at startup
  myServo.write(currentAngle);
  
  Serial.println("Slow Servo Control Initialized.");
  Serial.println("Type a target angle (0-180) and press Enter:");
}

void loop() {
  if (Serial.available() > 0) {
    int targetAngle = Serial.parseInt();

    // Clear any extra characters in the buffer
    while (Serial.available() > 0) {
      Serial.read();
    }

    // Validate input range
    if (targetAngle >= 0 && targetAngle <= 180) {
      Serial.print("Moving slowly from ");
      Serial.print(currentAngle);
      Serial.print(" to ");
      Serial.println(targetAngle);

      // Scenario A: Moving Forward (e.g., 0 to 180)
      if (currentAngle < targetAngle) {
        for (int pos = currentAngle; pos <= targetAngle; pos++) {
          myServo.write(pos);
          delay(speedDelay); 
        }
      } 
      // Scenario B: Moving Backward (e.g., 180 to 0)
      else if (currentAngle > targetAngle) {
        for (int pos = currentAngle; pos >= targetAngle; pos--) {
          myServo.write(pos);
          delay(speedDelay);
        }
      }

      // Update the current position tracking
      currentAngle = targetAngle;
      Serial.println("Target reached.");
      
    } else {
      Serial.println("Invalid! Enter an angle between 0 and 180.");
    }
  }
}
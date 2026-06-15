#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>  // Added Servo Library

// WiFi and API Config
const char* WIFI_SSID = "RumixonIoTech";
const char* WIFI_PASS = "pass1234";
const char* API_KEY = "cd_rum_140626_JbSUbf";
const char* serverName = "www.circuitdigest.cloud";
const char* serverPath = "/api/v1/waste-detection/detect";
const int serverPort = 443;

// Hardware Pins
#define TRIGGER_BTN 13
const int SERVO_PIN = 2; // Using safe GPIO 2 for servo

// Objects and Variables
Servo myServo;
int currentAngle = 80;       // Tracking current servo position
const int speedDelay = 30;   // Higher = slower servo movement
unsigned long lastTriggerTime = 0;
const unsigned long debounceDelay = 500;

// Camera Pin Mapping (AI Thinker)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

WiFiClientSecure client;

// Function to move servo at a slower pace
void moveServoSlowly(int targetAngle) {
  if (targetAngle < 0 || targetAngle > 180) return;
  
  Serial.print("Moving servo slowly to ");
  Serial.print(targetAngle);
  Serial.println(" degrees...");

  if (currentAngle < targetAngle) {
    for (int pos = currentAngle; pos <= targetAngle; pos++) {
      myServo.write(pos);
      delay(speedDelay); 
    }
  } 
  else if (currentAngle > targetAngle) {
    for (int pos = currentAngle; pos >= targetAngle; pos--) {
      myServo.write(pos);
      delay(speedDelay);
    }
  }
  currentAngle = targetAngle;
}

void initCamera() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0; cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = Y2_GPIO_NUM; cfg.pin_d1 = Y3_GPIO_NUM;
  cfg.pin_d2 = Y4_GPIO_NUM; cfg.pin_d3 = Y5_GPIO_NUM;
  cfg.pin_d4 = Y6_GPIO_NUM; cfg.pin_d5 = Y7_GPIO_NUM;
  cfg.pin_d6 = Y8_GPIO_NUM; cfg.pin_d7 = Y9_GPIO_NUM;
  cfg.pin_xclk = XCLK_GPIO_NUM; cfg.pin_pclk = PCLK_GPIO_NUM;
  cfg.pin_vsync = VSYNC_GPIO_NUM; cfg.pin_href = HREF_GPIO_NUM;
  cfg.pin_sscb_sda = SIOD_GPIO_NUM; cfg.pin_sscb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn = PWDN_GPIO_NUM; cfg.pin_reset = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = FRAMESIZE_VGA;
  cfg.jpeg_quality = 10;
  cfg.fb_count = 1;

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("Camera init failed!");
    while (1) delay(1000);
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_brightness(s, 1);
  s->set_contrast(s, 1);
  s->set_saturation(s, 0);
  s->set_whitebal(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_gain_ctrl(s, 1);
  Serial.println("Camera initialized.");
}

String sendImageToAPI(camera_fb_t* fb) {
  if (!client.connect(serverName, serverPort)) {
    return "Connection failed";
  }

  String boundary = "----ESP32CAMBoundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"imageFile\"; filename=\"snap.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  int contentLen = head.length() + fb->len + tail.length();

  client.println("POST " + String(serverPath) + " HTTP/1.1");
  client.println("Host: " + String(serverName));
  client.println("X-API-Key: " + String(API_KEY));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(contentLen));
  client.println("Connection: close");
  client.println();
  client.print(head);
  client.write(fb->buf, fb->len);
  client.print(tail);

  long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 15000) {
      client.stop();
      return "Timeout";
    }
  }

  String response = "";
  while (client.available()) {
    response += (char)client.read();
  }
  client.stop();

  // FIXED: Removed the broken multi-line string bug
  int jsonStart = response.indexOf("\r\n\r\n");
  return (jsonStart != -1) ? response.substring(jsonStart + 4) : response;
}

void classifyWaste() {
  camera_fb_t* fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);
  delay(200);
  fb = esp_camera_fb_get();
  if (!fb) { Serial.println("Capture failed"); return; }
  Serial.println("Photo captured! Sending to API...");
  String result = sendImageToAPI(fb);
  esp_camera_fb_return(fb);
  Serial.println("Response: " + result);

  // --- SERVO SEGREGATION LOGIC ---
  // Looks for keywords within the cloud server response text
  if (result.indexOf("plastic") != -1 || result.indexOf("Plastic") != -1) {
    Serial.println("Detected: PLASTIC");
    moveServoSlowly(45); // Adjust angle for your plastic bin
  } 
  else if (result.indexOf("paper") != -1 || result.indexOf("Paper") != -1) {
    Serial.println("Detected: PAPER");
    moveServoSlowly(90); // Adjust angle for your paper bin
  } 
  else if (result.indexOf("metal") != -1 || result.indexOf("Metal") != -1) {
    Serial.println("Detected: METAL");
    moveServoSlowly(135); // Adjust angle for your metal bin
  } 
  else {
    Serial.println("Detected: General / Unknown Waste");
    moveServoSlowly(0);  // Default resting position
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_BTN, INPUT_PULLUP);
  
  // Initialize Servo
  myServo.attach(SERVO_PIN);
  myServo.write(currentAngle); 

  initCamera();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // FIXED: Single line string to prevent compilation error
  Serial.println("\nConnected: " + WiFi.localIP().toString());
  client.setInsecure(); 
}

void loop() {
  if (digitalRead(TRIGGER_BTN) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastTriggerTime > debounceDelay) {
      lastTriggerTime = currentTime;
      Serial.println("Button pressed! Capturing image...");
      classifyWaste();
    }
  }
}
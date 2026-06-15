#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

const char* ssid = "Airtel_balu_2470_ex";
const char* password = "air38583";
const char* serverName = "www.circuitdigest.cloud";
const char* serverPath = "/api/v1/waste-detection/detect";
const int serverPort = 443;
const char* apiKey = "cd_rum_140626_JbSUbf";
const char* classes = "[]"; // You can give the classes according to your need.
const char* confidence = "0.2";

#define TRIGGER_BTN 13
unsigned long lastTriggerTime = 0;
const unsigned long debounceDelay = 500;

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

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\r\n", err);
    return;
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
  
  String boundary = "----ESP32Boundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"imageFile\"; filename=\"photo.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";
  
  String classesPart = "\r\n--" + boundary + "\r\n";
  classesPart += "Content-Disposition: form-data; name=\"classes\"\r\n\r\n";
  classesPart += String(classes) + "\r\n";
  
  String confPart = "--" + boundary + "\r\n";
  confPart += "Content-Disposition: form-data; name=\"confidence\"\r\n\r\n";
  confPart += String(confidence) + "\r\n";
  
  String tail = "--" + boundary + "--\r\n";
  
  int contentLen = head.length() + fb->len + classesPart.length() + confPart.length() + tail.length();
  
  client.println("POST " + String(serverPath) + " HTTP/1.1");
  client.println("Host: " + String(serverName));
  client.println("X-API-Key: " + String(apiKey));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(contentLen));
  client.println("Connection: close");
  client.println();
  client.print(head);
  client.write(fb->buf, fb->len);
  client.print(classesPart);
  client.print(confPart);
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
  
  int jsonStart = response.indexOf("\r\n\r\n");
  return (jsonStart != -1) ? response.substring(jsonStart + 4) : response;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_BTN, INPUT_PULLUP);
  initCamera();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWiFi connected: " + WiFi.localIP().toString());
  client.setInsecure();
}

void loop() {
  if (digitalRead(TRIGGER_BTN) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastTriggerTime > debounceDelay) {
      lastTriggerTime = currentTime;
      Serial.println("Button pressed! Capturing image...");
      
      camera_fb_t* fb = esp_camera_fb_get();
      esp_camera_fb_return(fb);
      delay(200);
      
      fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera capture failed");
        return;
      }
      
      Serial.println("Sending to Object Detection API...");
      String result = sendImageToAPI(fb);
      esp_camera_fb_return(fb);
      Serial.println("Response: " + result);
      
      int idx = result.indexOf("\"detection_count\":");
      if (idx != -1) {
        int valStart = result.indexOf(':', idx) + 1;
        int valEnd = result.indexOf(',', valStart);
        String count = result.substring(valStart, valEnd);
        count.trim();
        Serial.println("Objects detected: " + count);
      } else {
        Serial.println("No detection data found in response.");
      }
    }
  }
}
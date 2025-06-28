#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ESP32Servo.h>

// Version information
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_NAME "NexLock"

// Pin definitions for PN532 (I2C)
#define PN532_SDA 21
#define PN532_SCL 22
#define PN532_IRQ 19   // Optional interrupt pin
#define PN532_RESET 18 // Optional reset pin

// Other pin definitions
#define SERVO_PIN1 4
#define SERVO_PIN2 13
#define SERVO_PIN3 6
#define CONFIG_BUTTON_PIN 2

// Servo positions
#define LOCK_POSITION 0
#define OPEN_POSITION 90

// Timing constants (reduced intervals to save memory)
#define PING_INTERVAL 60000
#define STATUS_CHECK_INTERVAL 2000
#define AVAILABLE_BROADCAST_INTERVAL 15000
#define NFC_TIMEOUT 3000
#define CONFIG_BUTTON_HOLD_TIME 5000

// Network constants
#define DEFAULT_SERVER_PORT 3000
#define WIFI_CONNECTION_TIMEOUT 20
#define MAX_LOCKERS 3

// LCD constants
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// Memory optimization - smaller JSON buffers
#define SMALL_JSON_SIZE 256
#define MEDIUM_JSON_SIZE 512
#define LARGE_JSON_SIZE 1024

// PROGMEM strings to save RAM
const char HTML_HEADER[] PROGMEM = "<!DOCTYPE html><html><head><title>NexLock</title><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;margin:20px;background:#f0f0f0}.container{background:white;padding:15px;border-radius:5px}input{width:100%;padding:8px;margin:8px 0}button{background:#007bff;color:white;padding:12px;border:none;border-radius:3px;width:100%}</style></head><body><div class='container'>";

const char HTML_FORM[] PROGMEM = "<h2>WiFi Setup</h2><form action='/configure' method='POST'><label>SSID:</label><input type='text' name='ssid' required><label>Password:</label><input type='password' name='password' required><label>Server IP:</label><input type='text' name='serverIP' required><label>Port:</label><input type='number' name='serverPort' value='3000' required><button type='submit'>Configure</button></form>";

const char HTML_FOOTER[] PROGMEM = "</div></body></html>";

// Locker configuration structure
struct LockerConfig
{
  String lockerId;
  uint8_t servoPin;
  Servo *servo;
  uint8_t currentPosition;
  unsigned long lastStatusUpdate;
};

#endif
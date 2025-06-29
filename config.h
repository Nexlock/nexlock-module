#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version information
#define FIRMWARE_VERSION "1.2.0"
#define DEVICE_NAME "NexLock"

// Serial communication with Arduino
#define ARDUINO_RX_PIN 16
#define ARDUINO_TX_PIN 17
#define ARDUINO_BAUD_RATE 115200

// Other pin definitions
#define CONFIG_BUTTON_PIN 2

// Timing constants
#define PING_INTERVAL 60000
#define STATUS_CHECK_INTERVAL 2000
#define AVAILABLE_BROADCAST_INTERVAL 15000
#define CONFIG_BUTTON_HOLD_TIME 5000
#define SERIAL_TIMEOUT 1000

// Network constants
#define DEFAULT_SERVER_PORT 3000
#define WIFI_CONNECTION_TIMEOUT 20
#define MAX_LOCKERS 3

// Memory optimization - smaller JSON buffers
#define SMALL_JSON_SIZE 256
#define MEDIUM_JSON_SIZE 512
#define LARGE_JSON_SIZE 1024

// PROGMEM strings to save RAM
const char HTML_HEADER[] PROGMEM = "<!DOCTYPE html><html><head><title>NexLock</title><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;margin:20px;background:#f0f0f0}.container{background:white;padding:15px;border-radius:5px}input{width:100%;padding:8px;margin:8px 0}button{background:#007bff;color:white;padding:12px;border:none;border-radius:3px;width:100%}</style></head><body><div class='container'>";

const char HTML_FORM[] PROGMEM = "<h2>WiFi Setup</h2><form action='/configure' method='POST'><label>SSID:</label><input type='text' name='ssid' required><label>Password:</label><input type='password' name='password' required><label>Server IP:</label><input type='text' name='serverIP' required><label>Port:</label><input type='number' name='serverPort' value='3000' required><button type='submit'>Configure</button></form>";

const char HTML_FOOTER[] PROGMEM = "</div></body></html>";

// Serial command protocol
#define CMD_LOCK 'L'
#define CMD_UNLOCK 'U'
#define CMD_STATUS 'S'
#define CMD_ONLINE 'O'
#define CMD_OFFLINE 'F'

// Response codes from Arduino (simplified - only servo status)
#define RESP_LOCKED '1'
#define RESP_UNLOCKED '2'
#define RESP_ACK 'A'
#define RESP_ERROR 'E'

// Locker configuration structure
struct LockerConfig
{
  String lockerId;
  uint8_t lockerIndex;  // 1, 2, or 3 for Arduino
  String currentStatus; // "locked" or "unlocked"
  unsigned long lastStatusUpdate;
};

#endif
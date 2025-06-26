#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Servo.h>

// Version information
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_NAME "NexLock"

// Pin definitions
#define SS_PIN1 10
#define RST_PIN1 9
#define IR1 A0
#define IR2 A1
#define IR3 A2
#define SERVO_PIN1 4
#define SERVO_PIN2 5
#define SERVO_PIN3 6
#define CONFIG_BUTTON_PIN 2

// Servo positions
#define LOCK_POSITION 0
#define OPEN_POSITION 90

// Timing constants
#define PING_INTERVAL 30000
#define STATUS_CHECK_INTERVAL 1000
#define AVAILABLE_BROADCAST_INTERVAL 10000
#define NFC_TIMEOUT 5000
#define CONFIG_BUTTON_HOLD_TIME 5000

// IR sensor thresholds
#define IR_THRESHOLD_NORMAL 950
#define IR_THRESHOLD_LOCKER3 820

// Network constants
#define DEFAULT_SERVER_PORT 3000
#define WIFI_CONNECTION_TIMEOUT 30
#define MAX_LOCKERS 3

// LCD constants
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// Locker configuration structure
struct LockerConfig {
  String lockerId;
  int servoPin;
  Servo* servo;
  int irPin;
  int currentPosition;
  bool isOccupied;
  unsigned long lastStatusUpdate;
  int irThreshold;
};

#endif

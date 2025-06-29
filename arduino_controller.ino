/*
 * NexLock Arduino Controller
 *
 * This Arduino sketch controls the servo motors and LCD display
 * for the NexLock system. It communicates with the ESP32 module
 * via serial connection to receive lock/unlock commands and
 * report status updates.
 *
 * Hardware:
 * - 3 Servo motors (pins 4, 5, 6)
 * - LCD display (I2C address 0x27)
 * - Serial communication with ESP32 (pins 2, 3)
 *
 * Serial Protocol:
 * - Commands: L (lock), U (unlock), S (status), O (online), F (offline)
 * - Responses: 1 (locked), 2 (unlocked), A (ack), E (error)
 */

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// Communication with ESP32
#define ESP32_SERIAL_RX 2
#define ESP32_SERIAL_TX 3
#define ESP32_BAUD_RATE 115200

// Servo pins
#define SERVO_PIN1 4
#define SERVO_PIN2 5
#define SERVO_PIN3 6

// Servo positions
#define LOCK_POSITION 0
#define UNLOCK_POSITION 90

// LCD configuration
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// Command protocol (matching ESP32)
#define CMD_LOCK 'L'
#define CMD_UNLOCK 'U'
#define CMD_STATUS 'S'
#define CMD_ONLINE 'O'
#define CMD_OFFLINE 'F'

// Response codes
#define RESP_LOCKED '1'
#define RESP_UNLOCKED '2'
#define RESP_ACK 'A'
#define RESP_ERROR 'E'

// Global objects
SoftwareSerial esp32Serial(ESP32_SERIAL_RX, ESP32_SERIAL_TX);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
Servo servos[3];

// Locker states
struct LockerState
{
    bool isLocked;
    unsigned long lastUpdate;
};

LockerState lockers[3];
bool esp32Online = false;
unsigned long lastStatusSend = 0;

void setup()
{
    Serial.begin(115200);
    esp32Serial.begin(ESP32_BAUD_RATE);

    Serial.println("NexLock Arduino Controller v1.2.0");
    Serial.println("Initializing hardware...");

    // Initialize LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("NexLock Arduino");
    lcd.setCursor(0, 1);
    lcd.print("Initializing...");

    // Initialize servos - all start locked
    for (int i = 0; i < 3; i++)
    {
        servos[i].attach(SERVO_PIN1 + i);
        servos[i].write(LOCK_POSITION);
        lockers[i].isLocked = true;
        lockers[i].lastUpdate = 0;

        Serial.print("Servo ");
        Serial.print(i + 1);
        Serial.println(" initialized and locked");
    }

    delay(2000);
    updateLCD("System Ready", "Waiting for ESP32");

    Serial.println("Arduino Controller ready");
    Serial.println("Waiting for ESP32 connection...");
}

void loop()
{
    // Handle communication with ESP32
    handleESP32Communication();

    // Send status updates periodically when ESP32 is online
    unsigned long currentTime = millis();
    if (esp32Online && currentTime - lastStatusSend >= 2000)
    {
        sendAllStatus();
        lastStatusSend = currentTime;
    }

    delay(50);
}

void handleESP32Communication()
{
    if (esp32Serial.available() >= 2)
    {
        char command = esp32Serial.read();
        uint8_t lockerIndex = esp32Serial.read() - '0';

        Serial.print("Received command: ");
        Serial.print(command);
        Serial.print(" for locker: ");
        Serial.println(lockerIndex);

        switch (command)
        {
        case CMD_LOCK:
            lockLocker(lockerIndex);
            break;

        case CMD_UNLOCK:
            unlockLocker(lockerIndex);
            break;

        case CMD_STATUS:
            if (lockerIndex == 0)
            {
                sendAllStatus();
            }
            else
            {
                sendLockerStatus(lockerIndex);
            }
            break;

        case CMD_ONLINE:
            esp32Online = true;
            updateLCD("ESP32 Online", "System Ready");
            sendResponse(command, 0, RESP_ACK);
            Serial.println("ESP32 came online - system active");
            break;

        case CMD_OFFLINE:
            esp32Online = false;
            updateLCD("ESP32 Offline", "Local mode");
            sendResponse(command, 0, RESP_ACK);
            Serial.println("ESP32 went offline - local mode");
            break;

        default:
            Serial.print("Unknown command: ");
            Serial.println(command);
            sendResponse(command, lockerIndex, RESP_ERROR);
            break;
        }
    }
}

void lockLocker(uint8_t lockerIndex)
{
    if (lockerIndex < 1 || lockerIndex > 3)
    {
        Serial.print("Invalid locker index: ");
        Serial.println(lockerIndex);
        sendResponse(CMD_LOCK, lockerIndex, RESP_ERROR);
        return;
    }

    int arrayIndex = lockerIndex - 1;
    servos[arrayIndex].write(LOCK_POSITION);
    lockers[arrayIndex].isLocked = true;
    lockers[arrayIndex].lastUpdate = millis();

    updateLCD("Locker " + String(lockerIndex), "Locked");
    sendResponse(CMD_LOCK, lockerIndex, RESP_ACK);

    // Send status update after successful lock
    delay(100);
    sendResponse(CMD_STATUS, lockerIndex, RESP_LOCKED);

    Serial.print("Successfully locked locker ");
    Serial.println(lockerIndex);

    delay(1000);
    updateSystemStatus();
}

void unlockLocker(uint8_t lockerIndex)
{
    if (lockerIndex < 1 || lockerIndex > 3)
    {
        Serial.print("Invalid locker index: ");
        Serial.println(lockerIndex);
        sendResponse(CMD_UNLOCK, lockerIndex, RESP_ERROR);
        return;
    }

    int arrayIndex = lockerIndex - 1;
    servos[arrayIndex].write(UNLOCK_POSITION);
    lockers[arrayIndex].isLocked = false;
    lockers[arrayIndex].lastUpdate = millis();

    updateLCD("Locker " + String(lockerIndex), "Unlocked");
    sendResponse(CMD_UNLOCK, lockerIndex, RESP_ACK);

    // Send status update after successful unlock
    delay(100);
    sendResponse(CMD_STATUS, lockerIndex, RESP_UNLOCKED);

    Serial.print("Successfully unlocked locker ");
    Serial.println(lockerIndex);

    delay(1000);
    updateSystemStatus();
}

void sendLockerStatus(uint8_t lockerIndex)
{
    if (lockerIndex < 1 || lockerIndex > 3)
    {
        Serial.print("Cannot send status for invalid locker: ");
        Serial.println(lockerIndex);
        return;
    }

    int arrayIndex = lockerIndex - 1;
    char status = lockers[arrayIndex].isLocked ? RESP_LOCKED : RESP_UNLOCKED;

    sendResponse(CMD_STATUS, lockerIndex, status);

    Serial.print("Sent status for locker ");
    Serial.print(lockerIndex);
    Serial.print(": ");
    Serial.println(lockers[arrayIndex].isLocked ? "LOCKED" : "UNLOCKED");
}

void sendAllStatus()
{
    Serial.println("Sending status for all lockers...");
    for (int i = 1; i <= 3; i++)
    {
        sendLockerStatus(i);
        delay(50);
    }
}

void sendResponse(char command, uint8_t lockerIndex, char response)
{
    esp32Serial.write(command);
    esp32Serial.write('0' + lockerIndex);
    esp32Serial.write(response);
    esp32Serial.flush();

    Serial.print("Sent response to ESP32: ");
    Serial.print(command);
    Serial.print(lockerIndex);
    Serial.println(response);
}

void updateLCD(const String &line1, const String &line2)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1.substring(0, LCD_COLS));
    lcd.setCursor(0, 1);
    lcd.print(line2.substring(0, LCD_COLS));

    Serial.println("LCD: " + line1 + " | " + line2);
}

void updateSystemStatus()
{
    if (!esp32Online)
    {
        updateLCD("ESP32 Offline", "Local mode");
        return;
    }

    int lockedCount = 0;

    for (int i = 0; i < 3; i++)
    {
        if (lockers[i].isLocked)
            lockedCount++;
    }

    String status = "Locked: " + String(lockedCount) + "/3";
    updateLCD("System Ready", status);
}
sendLockerStatus(i);
delay(50);
}
}

void sendResponse(char command, uint8_t lockerIndex, char response)
{
    esp32Serial.write(command);
    esp32Serial.write('0' + lockerIndex);
    esp32Serial.write(response);
    esp32Serial.flush();

    Serial.print("Sent response: ");
    Serial.print(command);
    Serial.print(lockerIndex);
    Serial.println(response);
}

void updateLCD(const String &line1, const String &line2)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1.substring(0, LCD_COLS));
    lcd.setCursor(0, 1);
    lcd.print(line2.substring(0, LCD_COLS));
}

void updateSystemStatus()
{
    if (!esp32Online)
    {
        updateLCD("ESP32 Offline", "Local mode");
        return;
    }

    int lockedCount = 0;
    int occupiedCount = 0;

    for (int i = 0; i < 3; i++)
    {
        if (lockers[i].isLocked)
            lockedCount++;
        if (lockers[i].isOccupied)
            occupiedCount++;
    }

    String status = "L:" + String(lockedCount) + " O:" + String(occupiedCount);
    updateLCD("System Ready", status);
}

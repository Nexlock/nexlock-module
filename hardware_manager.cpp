#include "hardware_manager.h"

HardwareManager::HardwareManager(Preferences *prefs)
    : preferences(prefs), lockers(nullptr), numLockers(0), isConfigured(false),
      arduinoOnline(false), lastArduinoResponse(0), lastStatusRequest(0)
{
  arduinoSerial = &Serial2;
}

HardwareManager::~HardwareManager()
{
  if (lockers)
    delete[] lockers;
}

bool HardwareManager::initialize()
{
  // Initialize serial communication with Arduino
  arduinoSerial->begin(ARDUINO_BAUD_RATE, SERIAL_8N1, ARDUINO_RX_PIN, ARDUINO_TX_PIN);

  // Initialize config button
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  loadLockerConfiguration();

  // Notify Arduino that ESP32 is online
  setOnlineStatus(true);

  Serial.println("Hardware Manager initialized");
  return isConfigured;
}

void HardwareManager::loadLockerConfiguration()
{
  String moduleId = preferences->getString("moduleId", "");
  String ssid = preferences->getString("ssid", "");
  String serverIP = preferences->getString("serverIP", "");

  isConfigured = (moduleId.length() > 0 && ssid.length() > 0 && serverIP.length() > 0);

  if (isConfigured)
  {
    this->moduleId = moduleId;
    numLockers = preferences->getInt("numLockers", 0);

    if (numLockers > 0 && numLockers <= MAX_LOCKERS)
    {
      lockers = new LockerConfig[numLockers];

      for (int i = 0; i < numLockers; i++)
      {
        String key = "locker" + String(i);
        lockers[i].lockerId = preferences->getString(key.c_str(), "");
        lockers[i].lockerIndex = i + 1;      // Arduino uses 1-based indexing
        lockers[i].currentStatus = "locked"; // Default status
        lockers[i].lastStatusUpdate = 0;

        Serial.print("Configured locker: ");
        Serial.println(lockers[i].lockerId);
      }
    }
  }
}

void HardwareManager::saveLockerConfiguration(const String &moduleId, const String *lockerIds, int count)
{
  if (moduleId.length() == 0 || count <= 0 || count > MAX_LOCKERS)
  {
    Serial.println("ERROR: Invalid configuration parameters");
    return;
  }

  preferences->putString("moduleId", moduleId);
  preferences->putInt("numLockers", count);

  for (int i = 0; i < count; i++)
  {
    String key = "locker" + String(i);
    preferences->putString(key.c_str(), lockerIds[i]);
  }

  // Update local configuration
  this->moduleId = moduleId;
  this->isConfigured = true;

  Serial.println("Configuration saved successfully");
}

bool HardwareManager::unlockLocker(const String &lockerId)
{
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].lockerId == lockerId)
    {
      sendCommandToArduino(CMD_UNLOCK, lockers[i].lockerIndex);
      if (waitForArduinoResponse(RESP_ACK))
      {
        lockers[i].currentStatus = "unlocked";
        lockers[i].lastStatusUpdate = millis();
        Serial.print("Unlocked locker: ");
        Serial.println(lockerId);
        return true;
      }
      else
      {
        Serial.print("Failed to unlock locker: ");
        Serial.println(lockerId);
        return false;
      }
    }
  }
  return false;
}

bool HardwareManager::lockLocker(const String &lockerId)
{
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].lockerId == lockerId)
    {
      sendCommandToArduino(CMD_LOCK, lockers[i].lockerIndex);
      if (waitForArduinoResponse(RESP_ACK))
      {
        lockers[i].currentStatus = "locked";
        lockers[i].lastStatusUpdate = millis();
        Serial.print("Locked locker: ");
        Serial.println(lockerId);
        return true;
      }
      else
      {
        Serial.print("Failed to lock locker: ");
        Serial.println(lockerId);
        return false;
      }
    }
  }
  return false;
}

void HardwareManager::requestStatusUpdate()
{
  unsigned long currentTime = millis();
  if (currentTime - lastStatusRequest < STATUS_CHECK_INTERVAL)
    return;

  lastStatusRequest = currentTime;
  sendCommandToArduino(CMD_STATUS, 0); // Request status for all lockers
}

void HardwareManager::loop()
{
  // Process incoming serial data from Arduino
  while (arduinoSerial->available())
  {
    char command = arduinoSerial->read();
    delay(10); // Small delay to ensure next byte is available

    if (arduinoSerial->available())
    {
      uint8_t lockerIndex = arduinoSerial->read() - '0'; // Convert char to number
      delay(10);

      if (arduinoSerial->available())
      {
        char response = arduinoSerial->read();
        processArduinoMessage(command, lockerIndex, response);
        lastArduinoResponse = millis();
        arduinoOnline = true;
      }
    }
  }

  // Check Arduino connection
  if (millis() - lastArduinoResponse > 10000) // 10 seconds timeout
  {
    if (arduinoOnline)
    {
      Serial.println("Arduino connection lost");
      arduinoOnline = false;
    }
  }

  // Request periodic status updates
  requestStatusUpdate();
}

void HardwareManager::sendCommandToArduino(char command, uint8_t lockerIndex)
{
  arduinoSerial->write(command);
  arduinoSerial->write('0' + lockerIndex); // Convert number to char
  arduinoSerial->flush();

  Serial.print("Sent to Arduino: ");
  Serial.print(command);
  Serial.println(lockerIndex);
}

bool HardwareManager::waitForArduinoResponse(char expectedResponse, unsigned long timeout)
{
  unsigned long startTime = millis();

  while (millis() - startTime < timeout)
  {
    if (arduinoSerial->available() >= 3) // Command + Index + Response
    {
      char cmd = arduinoSerial->read();
      uint8_t idx = arduinoSerial->read() - '0';
      char resp = arduinoSerial->read();

      if (resp == expectedResponse)
      {
        processArduinoMessage(cmd, idx, resp);
        return true;
      }
    }
    delay(10);
  }

  return false;
}

void HardwareManager::processArduinoMessage(char command, uint8_t lockerIndex, char response)
{
  Serial.print("Arduino response: ");
  Serial.print(command);
  Serial.print(lockerIndex);
  Serial.println(response);

  // Update locker status based on response (simplified for servo status only)
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].lockerIndex == lockerIndex)
    {
      switch (response)
      {
      case RESP_LOCKED:
        lockers[i].currentStatus = "locked";
        break;
      case RESP_UNLOCKED:
        lockers[i].currentStatus = "unlocked";
        break;
      case RESP_ACK:
        // Acknowledgment received, status will be updated by subsequent status response
        break;
      case RESP_ERROR:
        Serial.print("Error reported for locker: ");
        Serial.println(lockerIndex);
        break;
      }
      lockers[i].lastStatusUpdate = millis();
      break;
    }
  }
}

void HardwareManager::setOnlineStatus(bool online)
{
  char command = online ? CMD_ONLINE : CMD_OFFLINE;
  sendCommandToArduino(command, 0);
}

bool HardwareManager::checkConfigButton()
{
  static bool buttonPressed = false;
  static unsigned long pressStart = 0;

  if (digitalRead(CONFIG_BUTTON_PIN) == LOW)
  {
    if (!buttonPressed)
    {
      buttonPressed = true;
      pressStart = millis();
    }
    else if (millis() - pressStart > CONFIG_BUTTON_HOLD_TIME)
    {
      buttonPressed = false;
      return true;
    }
  }
  else
  {
    buttonPressed = false;
  }

  return false;
}

String HardwareManager::getModuleId() const
{
  return moduleId;
}

String HardwareManager::getLockerStatus(const String &lockerId) const
{
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].lockerId == lockerId)
    {
      return lockers[i].currentStatus;
    }
  }
  return "unknown";
}
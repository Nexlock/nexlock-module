#include "hardware_manager.h"

HardwareManager::HardwareManager(Preferences *prefs)
    : preferences(prefs), lockers(nullptr), numLockers(0), isConfigured(false),
      waitingForValidation(false), nfcScanTime(0)
{

  nfc = new Adafruit_PN532(PN532_SDA, PN532_SCL);
  lcd = new LiquidCrystal_I2C(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
}

HardwareManager::~HardwareManager()
{
  delete nfc;
  delete lcd;
  if (lockers)
    delete[] lockers;
}

bool HardwareManager::initialize()
{
  // Initialize I2C
  Wire.begin(PN532_SDA, PN532_SCL);

  // Initialize LCD - handle missing hardware gracefully
  bool lcdInitialized = false;
  try
  {
    lcd->init();
    lcd->backlight();
    lcdInitialized = true;
    Serial.println("LCD initialized successfully");
  }
  catch (...)
  {
    Serial.println("Warning: LCD not found, continuing without display");
  }

  // Initialize config button
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  // Allow allocation of all timers for servo library
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  loadLockerConfiguration();

  if (isConfigured)
  {
    // Initialize PN532 - handle missing hardware gracefully
    bool nfcInitialized = false;
    nfc->begin();

    // Check if PN532 is connected
    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata)
    {
      Serial.println("Warning: PN532 not found, NFC functionality disabled");
      nfcInitialized = false;
    }
    else
    {
      Serial.print("Found PN532 with firmware version: 0x");
      Serial.println(versiondata, HEX);
      nfc->SAMConfig();
      nfcInitialized = true;
    }

    initializeServos();

    if (lcdInitialized)
    {
      updateLCD("System Ready", "Hardware OK");
    }
    else
    {
      Serial.println("System Ready - LCD not available");
    }
    return true;
  }

  if (lcdInitialized)
  {
    updateLCD("WiFi Connected", "Awaiting config");
  }
  else
  {
    Serial.println("WiFi Connected - Awaiting config");
  }
  return false;
}

void HardwareManager::loadLockerConfiguration()
{
  String moduleId = preferences->getString("moduleId", "");
  String ssid = preferences->getString("ssid", "");
  String serverIP = preferences->getString("serverIP", "");

  // Device is configured only if it has WiFi credentials AND module ID
  isConfigured = (moduleId.length() > 0 && ssid.length() > 0 && serverIP.length() > 0);

  Serial.print(F("Configuration check: moduleId="));
  Serial.print(moduleId);
  Serial.print(F(" (length: "));
  Serial.print(moduleId.length());
  Serial.print(F("), ssid="));
  Serial.print(ssid);
  Serial.print(F(", serverIP="));
  Serial.print(serverIP);
  Serial.print(F(", isConfigured="));
  Serial.println(isConfigured);

  if (isConfigured)
  {
    this->moduleId = moduleId;
    numLockers = preferences->getInt("numLockers", 0);

    Serial.print(F("Module configured with ID: "));
    Serial.println(moduleId);
    Serial.print(F("Number of lockers: "));
    Serial.println(numLockers);

    if (numLockers > 0 && numLockers <= MAX_LOCKERS)
    {
      lockers = new LockerConfig[numLockers];

      for (int i = 0; i < numLockers; i++)
      {
        String key = "locker" + String(i);
        lockers[i].lockerId = preferences->getString(key.c_str(), "");

        // Assign hardware based on index
        switch (i)
        {
        case 0:
          lockers[i].servoPin = SERVO_PIN1;
          lockers[i].servo = &servo1;
          break;
        case 1:
          lockers[i].servoPin = SERVO_PIN2;
          lockers[i].servo = &servo2;
          break;
        case 2:
          lockers[i].servoPin = SERVO_PIN3;
          lockers[i].servo = &servo3;
          break;
        }

        lockers[i].currentPosition = LOCK_POSITION;
        lockers[i].lastStatusUpdate = 0;

        Serial.print(F("Configured locker: "));
        Serial.println(lockers[i].lockerId);
      }
    }
  }
  else
  {
    Serial.println(F("Module not configured yet - missing one or more required values"));
  }
}

void HardwareManager::saveLockerConfiguration(const String &moduleId, const String *lockerIds, int count)
{
  Serial.print(F("Saving module configuration: moduleId="));
  Serial.print(moduleId);
  Serial.print(F(", numLockers="));
  Serial.println(count);

  // Verify we have valid data before saving
  if (moduleId.length() == 0)
  {
    Serial.println(F("ERROR: Cannot save empty moduleId"));
    return;
  }

  if (count <= 0 || count > MAX_LOCKERS)
  {
    Serial.print(F("ERROR: Invalid locker count: "));
    Serial.println(count);
    return;
  }

  preferences->putString("moduleId", moduleId);
  preferences->putInt("numLockers", count);

  for (int i = 0; i < count && i < MAX_LOCKERS; i++)
  {
    String key = "locker" + String(i);
    preferences->putString(key.c_str(), lockerIds[i]);
    Serial.print(F("Saved locker: "));
    Serial.print(key);
    Serial.print(F(" = "));
    Serial.println(lockerIds[i]);
  }

  // Verify the save by reading back
  String savedModuleId = preferences->getString("moduleId", "");
  int savedNumLockers = preferences->getInt("numLockers", 0);

  Serial.print(F("Verification - saved moduleId: "));
  Serial.println(savedModuleId);
  Serial.print(F("Verification - saved numLockers: "));
  Serial.println(savedNumLockers);

  if (savedModuleId == moduleId && savedNumLockers == count)
  {
    // Update local configuration status
    this->moduleId = moduleId;
    this->isConfigured = true;
    Serial.println(F("Configuration saved and verified successfully"));
  }
  else
  {
    Serial.println(F("ERROR: Configuration verification failed"));
  }
}

void HardwareManager::initializeServos()
{
  for (int i = 0; i < numLockers; i++)
  {
    lockers[i].servo->attach(lockers[i].servoPin);
    lockers[i].servo->write(LOCK_POSITION);
    lockers[i].currentPosition = LOCK_POSITION;
  }
}

bool HardwareManager::scanNFC(String &nfcCode)
{
  if (!isConfigured)
    return false;

  // Check if NFC hardware is available
  static bool nfcAvailable = true;
  if (!nfcAvailable)
  {
    return false;
  }

  try
  {
    if (readNFCCard(nfcCode))
    {
      Serial.print(F("NFC: "));
      Serial.println(nfcCode);
      updateLCD(F("NFC Detected"), nfcCode.substring(0, 12));
      delay(1500);
      updateSystemStatus();
      return true;
    }
  }
  catch (...)
  {
    Serial.println("NFC communication failed, disabling NFC");
    nfcAvailable = false;
  }

  return false;
}

bool HardwareManager::readNFCCard(String &nfcCode)
{
  uint8_t success;
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};
  uint8_t uidLength;

  // Wait for an ISO14443A type cards (Mifare, etc.)
  success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);

  if (success)
  {
    // Build hex string from UID
    nfcCode = "";
    for (uint8_t i = 0; i < uidLength; i++)
    {
      if (uid[i] < 0x10)
        nfcCode += "0";
      nfcCode += String(uid[i], HEX);
    }
    nfcCode.toUpperCase();
    return true;
  }

  return false;
}

void HardwareManager::setNFCValidationResult(bool valid, const String &message)
{
  // This method is no longer used in the new server-driven flow
  // Keep for backward compatibility
  if (valid)
  {
    updateLCD("Access Granted", message);
  }
  else
  {
    updateLCD("Access Denied", message);
  }

  delay(2000);
  updateSystemStatus();
}

void HardwareManager::resetNFCValidation()
{
  waitingForValidation = false;
  currentNFCCode = "";
}

void HardwareManager::unlockLocker(const String &lockerId)
{
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].lockerId == lockerId)
    {
      lockers[i].servo->write(OPEN_POSITION);
      lockers[i].currentPosition = OPEN_POSITION;

      updateLCD(F("Unlocked"), "L" + lockerId);
      Serial.print(F("Unlocked: "));
      Serial.println(lockerId);

      delay(1500);
      updateSystemStatus();
      break;
    }
  }
}

void HardwareManager::lockLocker(const String &lockerId)
{
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].lockerId == lockerId)
    {
      lockers[i].servo->write(LOCK_POSITION);
      lockers[i].currentPosition = LOCK_POSITION;

      updateLCD(F("Locked"), "L" + lockerId);
      Serial.print(F("Locked: "));
      Serial.println(lockerId);

      delay(1500);
      updateSystemStatus();
      break;
    }
  }
}

void HardwareManager::toggleLocker(const String &lockerId)
{
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].lockerId == lockerId)
    {
      if (lockers[i].currentPosition == LOCK_POSITION)
      {
        lockers[i].servo->write(OPEN_POSITION);
        lockers[i].currentPosition = OPEN_POSITION;
        updateLCD("Opened", "Locker " + lockerId);
      }
      else
      {
        lockers[i].servo->write(LOCK_POSITION);
        lockers[i].currentPosition = LOCK_POSITION;
        updateLCD("Locked", "Locker " + lockerId);
      }

      Serial.println("Locker " + lockerId + " toggled");
      break;
    }
  }
}

void HardwareManager::updateLCD(const String &line1, const String &line2)
{
  // Only update LCD if it was successfully initialized
  static bool lcdAvailable = true;

  if (!lcdAvailable)
  {
    Serial.println("LCD: " + line1 + " | " + line2);
    return;
  }

  try
  {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(line1.substring(0, LCD_COLS));
    lcd->setCursor(0, 1);
    lcd->print(line2.substring(0, LCD_COLS));
  }
  catch (...)
  {
    Serial.println("LCD communication failed, disabling LCD");
    lcdAvailable = false;
    Serial.println("LCD: " + line1 + " | " + line2);
  }
}

void HardwareManager::updateLCD(const __FlashStringHelper *line1, const __FlashStringHelper *line2)
{
  // Only update LCD if it was successfully initialized
  static bool lcdAvailable = true;

  if (!lcdAvailable)
  {
    Serial.print("LCD: ");
    Serial.print(line1);
    Serial.print(" | ");
    Serial.println(line2);
    return;
  }

  try
  {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(line1);
    lcd->setCursor(0, 1);
    lcd->print(line2);
  }
  catch (...)
  {
    Serial.println("LCD communication failed, disabling LCD");
    lcdAvailable = false;
    Serial.print("LCD: ");
    Serial.print(line1);
    Serial.print(" | ");
    Serial.println(line2);
  }
}

void HardwareManager::updateSystemStatus()
{
  if (!isConfigured)
  {
    updateLCD(F("WiFi Connected"), F("Awaiting config"));
    return;
  }

  int openCount = 0;
  for (int i = 0; i < numLockers; i++)
  {
    if (lockers[i].currentPosition == OPEN_POSITION)
      openCount++;
  }

  updateLCD("Open:" + String(openCount), F("Ready"));
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
      return true; // Factory reset requested
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
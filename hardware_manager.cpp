#include "hardware_manager.h"

HardwareManager::HardwareManager(Preferences* prefs) 
  : preferences(prefs), lockers(nullptr), numLockers(0), isConfigured(false),
    waitingForValidation(false), nfcScanTime(0) {
  
  pn532i2c = new PN532_I2C(Wire);
  nfc = new PN532(*pn532i2c);
  nfcAdapter = new NfcAdapter(*nfc);
  lcd = new LiquidCrystal_I2C(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
}

HardwareManager::~HardwareManager() {
  delete nfcAdapter;
  delete nfc;
  delete pn532i2c;
  delete lcd;
  if (lockers) delete[] lockers;
}

bool HardwareManager::initialize() {
  // Initialize I2C
  Wire.begin(PN532_SDA, PN532_SCL);
  
  // Initialize LCD
  lcd->init();
  lcd->backlight();
  
  // Initialize config button
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  
  // Optional: Initialize reset pin if used
  if (PN532_RESET != -1) {
    pinMode(PN532_RESET, OUTPUT);
    digitalWrite(PN532_RESET, HIGH);
  }
  
  loadLockerConfiguration();
  
  if (isConfigured) {
    // Initialize PN532
    nfcAdapter->begin();
    
    // Check if PN532 is connected
    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata) {
      Serial.println("PN532 not found - check I2C wiring");
      updateLCD("NFC Error", "Check I2C wiring");
      delay(2000);
    } else {
      Serial.print("Found PN532 with firmware version: ");
      Serial.println(versiondata, HEX);
      
      // Configure board to read RFID tags
      nfc->SAMConfig();
    }
    
    initializeServos();
    initializeIRSensors();
    
    updateLCD("System Ready", "Configured");
    return true;
  }
  
  updateLCD("WiFi Connected", "Awaiting config");
  return false;
}

void HardwareManager::loadLockerConfiguration() {
  String moduleId = preferences->getString("moduleId", "");
  isConfigured = (moduleId.length() > 0);
  
  if (isConfigured) {
    numLockers = preferences->getInt("numLockers", 0);
    if (numLockers > 0 && numLockers <= MAX_LOCKERS) {
      lockers = new LockerConfig[numLockers];
      
      for (int i = 0; i < numLockers; i++) {
        String key = "locker" + String(i);
        lockers[i].lockerId = preferences->getString(key.c_str(), "");
        
        // Assign hardware based on index
        switch(i) {
          case 0:
            lockers[i].servoPin = SERVO_PIN1;
            lockers[i].servo = &servo1;
            lockers[i].irPin = IR1;
            lockers[i].irThreshold = IR_THRESHOLD_NORMAL;
            break;
          case 1:
            lockers[i].servoPin = SERVO_PIN2;
            lockers[i].servo = &servo2;
            lockers[i].irPin = IR2;
            lockers[i].irThreshold = IR_THRESHOLD_NORMAL;
            break;
          case 2:
            lockers[i].servoPin = SERVO_PIN3;
            lockers[i].servo = &servo3;
            lockers[i].irPin = IR3;
            lockers[i].irThreshold = IR_THRESHOLD_LOCKER3;
            break;
        }
        
        lockers[i].currentPosition = LOCK_POSITION;
        lockers[i].isOccupied = false;
        lockers[i].lastStatusUpdate = 0;
      }
    }
  }
}

void HardwareManager::saveLockerConfiguration(const String& moduleId, const String* lockerIds, int count) {
  preferences->putString("moduleId", moduleId);
  preferences->putInt("numLockers", count);
  
  for (int i = 0; i < count && i < MAX_LOCKERS; i++) {
    String key = "locker" + String(i);
    preferences->putString(key.c_str(), lockerIds[i]);
  }
}

void HardwareManager::initializeServos() {
  for (int i = 0; i < numLockers; i++) {
    lockers[i].servo->attach(lockers[i].servoPin);
    lockers[i].servo->write(LOCK_POSITION);
    lockers[i].currentPosition = LOCK_POSITION;
  }
}

void HardwareManager::initializeIRSensors() {
  for (int i = 0; i < numLockers; i++) {
    pinMode(lockers[i].irPin, INPUT);
  }
}

bool HardwareManager::scanNFC(String& nfcCode) {
  if (!isConfigured) return false;
  
  if (readNFCText(nfcCode)) {
    currentNFCCode = nfcCode;
    waitingForValidation = true;
    nfcScanTime = millis();
    
    updateLCD("Validating...", "Please wait");
    Serial.println("NFC text read: " + nfcCode);
    return true;
  }
  
  // Handle timeout
  if (waitingForValidation && (millis() - nfcScanTime > NFC_TIMEOUT)) {
    resetNFCValidation();
    updateLCD("NFC Timeout", "Try again");
    delay(2000);
    updateSystemStatus();
  }
  
  return false;
}

bool HardwareManager::readNFCText(String& nfcText) {
  if (nfcAdapter->tagPresent()) {
    NfcTag tag = nfcAdapter->read();
    
    if (tag.hasNdefMessage()) {
      NdefMessage message = tag.getNdefMessage();
      
      // Look for text records
      for (int i = 0; i < message.getRecordCount(); i++) {
        NdefRecord record = message.getRecord(i);
        
        if (record.getTnf() == TNF_WELL_KNOWN && record.getType() == "T") {
          // Text record found
          int payloadLength = record.getPayloadLength();
          byte payload[payloadLength];
          record.getPayload(payload);
          
          // First byte is status (language code length + encoding)
          int languageCodeLength = payload[0] & 0x3F;
          
          // Extract text after language code
          nfcText = "";
          for (int j = 1 + languageCodeLength; j < payloadLength; j++) {
            nfcText += (char)payload[j];
          }
          
          return true;
        }
      }
    } else {
      // If no NDEF message, try to read UID as fallback
      String uid = "";
      byte uidLength;
      byte uid_bytes[7];
      
      if (nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid_bytes, &uidLength)) {
        for (uint8_t i = 0; i < uidLength; i++) {
          if (uid_bytes[i] < 0x10) uid += "0";
          uid += String(uid_bytes[i], HEX);
        }
        nfcText = uid;
        return true;
      }
    }
  }
  
  return false;
}

void HardwareManager::setNFCValidationResult(bool valid, const String& message) {
  if (!waitingForValidation) return;
  
  waitingForValidation = false;
  
  if (valid) {
    updateLCD("Access Granted", message);
  } else {
    updateLCD("Access Denied", message);
  }
  
  delay(2000);
  updateSystemStatus();
}

void HardwareManager::resetNFCValidation() {
  waitingForValidation = false;
  currentNFCCode = "";
}

bool HardwareManager::checkIRSensor(int pin, int threshold) {
  int value = analogRead(pin);
  return value < threshold;
}

bool HardwareManager::checkLockerStatuses() {
  if (!isConfigured) return false;
  
  bool statusChanged = false;
  
  for (int i = 0; i < numLockers; i++) {
    bool wasOccupied = lockers[i].isOccupied;
    bool isOccupied = checkIRSensor(lockers[i].irPin, lockers[i].irThreshold);
    
    lockers[i].isOccupied = isOccupied;
    
    if (wasOccupied != isOccupied) {
      statusChanged = true;
      Serial.printf("Locker %s status changed: %s\n", 
                   lockers[i].lockerId.c_str(), isOccupied ? "Occupied" : "Empty");
    }
  }
  
  return statusChanged;
}

void HardwareManager::unlockLocker(const String& lockerId) {
  for (int i = 0; i < numLockers; i++) {
    if (lockers[i].lockerId == lockerId) {
      lockers[i].servo->write(OPEN_POSITION);
      lockers[i].currentPosition = OPEN_POSITION;
      
      updateLCD("Unlocked", "Locker " + lockerId);
      Serial.println("Locker " + lockerId + " unlocked");
      
      delay(2000);
      updateSystemStatus();
      break;
    }
  }
}

void HardwareManager::toggleLocker(const String& lockerId) {
  for (int i = 0; i < numLockers; i++) {
    if (lockers[i].lockerId == lockerId) {
      if (lockers[i].currentPosition == LOCK_POSITION) {
        lockers[i].servo->write(OPEN_POSITION);
        lockers[i].currentPosition = OPEN_POSITION;
        updateLCD("Opened", "Locker " + lockerId);
      } else {
        lockers[i].servo->write(LOCK_POSITION);
        lockers[i].currentPosition = LOCK_POSITION;
        updateLCD("Locked", "Locker " + lockerId);
      }
      
      Serial.println("Locker " + lockerId + " toggled");
      break;
    }
  }
}

void HardwareManager::updateLCD(const String& line1, const String& line2) {
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print(line1);
  lcd->setCursor(0, 1);
  lcd->print(line2);
}

void HardwareManager::updateSystemStatus() {
  if (!isConfigured) {
    updateLCD("WiFi Connected", "Awaiting config");
    return;
  }
  
  int openCount = 0;
  int occupiedCount = 0;
  
  for (int i = 0; i < numLockers; i++) {
    if (lockers[i].currentPosition == OPEN_POSITION) openCount++;
    if (lockers[i].isOccupied) occupiedCount++;
  }
  
  updateLCD("Open:" + String(openCount) + " Occ:" + String(occupiedCount), "Scan NFC card");
}

bool HardwareManager::checkConfigButton() {
  static bool buttonPressed = false;
  static unsigned long pressStart = 0;
  
  if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      pressStart = millis();
    } else if (millis() - pressStart > CONFIG_BUTTON_HOLD_TIME) {
      buttonPressed = false;
      return true; // Factory reset requested
    }
  } else {
    buttonPressed = false;
  }
  
  return false;
}

String HardwareManager::getModuleId() const {
  return preferences->getString("moduleId", "");
}

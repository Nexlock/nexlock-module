#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include "config.h"

class HardwareManager {
private:
  PN532_I2C* pn532i2c;
  PN532* nfc;
  NfcAdapter* nfcAdapter;
  LiquidCrystal_I2C* lcd;
  Preferences* preferences;
  
  LockerConfig* lockers;
  int numLockers;
  bool isConfigured;
  String moduleId;
  
  // NFC validation state
  bool waitingForValidation;
  String currentNFCCode;
  unsigned long nfcScanTime;
  
  // Servo instances
  Servo servo1, servo2, servo3;
  
  void initializeServos();
  void initializeIRSensors();
  bool checkIRSensor(int pin, int threshold);
  bool readNFCText(String& nfcText);

public:
  HardwareManager(Preferences* prefs);
  ~HardwareManager();
  
  bool initialize();
  void loadLockerConfiguration();
  void saveLockerConfiguration(const String& moduleId, const String* lockerIds, int count);
  
  // NFC operations
  bool scanNFC(String& nfcCode);
  void setNFCValidationResult(bool valid, const String& message);
  bool isWaitingForNFCValidation() const { return waitingForValidation; }
  void resetNFCValidation();
  
  // Locker operations
  void unlockLocker(const String& lockerId);
  void toggleLocker(const String& lockerId);
  bool checkLockerStatuses();
  
  // LCD operations
  void updateLCD(const String& line1, const String& line2);
  void updateSystemStatus();
  
  // Configuration button
  bool checkConfigButton();
  
  // Getters
  int getNumLockers() const { return numLockers; }
  LockerConfig* getLockers() const { return lockers; }
  bool getConfigurationStatus() const { return isConfigured; }
  String getModuleId() const { return moduleId; }
};

#endif

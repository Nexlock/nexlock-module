#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <HardwareSerial.h>
#include <Preferences.h>
#include "config.h"

class HardwareManager
{
private:
  HardwareSerial *arduinoSerial;
  Preferences *preferences;

  LockerConfig *lockers;
  int numLockers;
  bool isConfigured;
  String moduleId;

  // Arduino communication state
  bool arduinoOnline;
  unsigned long lastArduinoResponse;
  unsigned long lastStatusRequest;

  // Serial communication methods
  void sendCommandToArduino(char command, uint8_t lockerIndex);
  bool waitForArduinoResponse(char expectedResponse, unsigned long timeout = SERIAL_TIMEOUT);
  void processArduinoMessage(char command, uint8_t lockerIndex, char response);

public:
  HardwareManager(Preferences *prefs);
  ~HardwareManager();

  bool initialize();
  void loadLockerConfiguration();
  void saveLockerConfiguration(const String &moduleId, const String *lockerIds, int count);

  // Locker operations
  bool unlockLocker(const String &lockerId);
  bool lockLocker(const String &lockerId);
  void requestStatusUpdate();

  // Arduino communication
  void loop(); // Process serial communication
  void setOnlineStatus(bool online);
  bool isArduinoOnline() const { return arduinoOnline; }

  // Configuration button
  bool checkConfigButton();

  // Getters
  int getNumLockers() const { return numLockers; }
  LockerConfig *getLockers() const { return lockers; }
  bool getConfigurationStatus() const { return isConfigured; }
  String getModuleId() const;
  String getLockerStatus(const String &lockerId) const;
};

#endif

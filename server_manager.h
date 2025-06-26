#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <SocketIOclient.h>
#include <ArduinoJson.h>
#include "config.h"

// Forward declaration to avoid circular dependency
class HardwareManager;

class ServerManager {
private:
  SocketIOclient* socketIO;
  HardwareManager* hardware;
  String moduleId;
  String macAddress;
  bool isConnected;
  bool isConfigured;
  
  unsigned long lastPing;
  unsigned long lastAvailableBroadcast;

  void handleModuleConfiguration(uint8_t* payload, size_t length);
  void handleUnlockEvent(uint8_t* payload, size_t length);
  void handleNFCValidationResult(uint8_t* payload, size_t length);

public:
  ServerManager(HardwareManager* hw, const String& mac);
  ~ServerManager();
  
  bool initialize(const String& serverIP, int serverPort);
  void loop();
  
  void broadcastAvailable();
  void registerModule();
  void validateNFC(const String& nfcCode);
  void sendLockerStatus(const String& lockerId, bool isOccupied);
  void sendPing();
  
  bool getConnectionStatus() const { return isConnected; }
  bool getConfigurationStatus() const { return isConfigured; }
  
  static void socketIOEventWrapper(socketIOmessageType_t type, uint8_t* payload, size_t length);
  void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length);
  
  static ServerManager* instance; // For static callback
};

#endif

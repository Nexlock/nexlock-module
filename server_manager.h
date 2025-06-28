#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "config.h"

// Forward declaration to avoid circular dependency
class HardwareManager;

using namespace websockets;

class ServerManager
{
private:
  WebsocketsClient *webSocket;
  HardwareManager *hardware;
  String moduleId;
  String macAddress;
  String serverURL;
  bool isConnected;
  bool isConfigured;

  unsigned long lastPing;
  unsigned long lastReconnectAttempt;

  void handleMessage(const String &message);
  void handleModuleConfiguration(const JsonDocument &doc);
  void handleLockUnlockCommand(const JsonDocument &doc);
  bool reconnect();

public:
  ServerManager(HardwareManager *hw, const String &mac);
  ~ServerManager();

  bool initialize(const String &serverIP, int serverPort);
  void loop();

  void registerModule();
  void sendStatusUpdate(const String &lockerId, const String &status);
  void sendLockerStatus(const String &lockerId, bool isOccupied);
  void sendPing();

  bool getConnectionStatus() const { return isConnected; }
  bool getConfigurationStatus() const { return isConfigured; }

  static ServerManager *instance; // For static callback
};

#endif

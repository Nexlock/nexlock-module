#include "server_manager.h"
#include "hardware_manager.h"

ServerManager *ServerManager::instance = nullptr;

ServerManager::ServerManager(HardwareManager *hw, const String &mac)
    : hardware(hw), macAddress(mac), isConnected(false), isConfigured(false),
      lastPing(0), lastReconnectAttempt(0)
{
  webSocket = new WebsocketsClient();
  instance = this;
}

ServerManager::~ServerManager()
{
  if (webSocket)
  {
    webSocket->close();
    delete webSocket;
  }
  instance = nullptr;
}

bool ServerManager::initialize(const String &serverIP, int serverPort)
{
  moduleId = hardware->getModuleId();
  isConfigured = hardware->getConfigurationStatus();

  // Construct WebSocket URL for raw WebSocket connection (not Socket.IO)
  serverURL = "ws://" + serverIP + ":" + String(serverPort) + "/ws";

  // Set up WebSocket event handlers
  webSocket->onMessage([this](WebsocketsMessage message)
                       { handleMessage(message.data()); });

  webSocket->onEvent([this](WebsocketsEvent event, String data)
                     {
    switch(event) {
      case WebsocketsEvent::ConnectionOpened:
        Serial.println("WebSocket Connected to server");
        isConnected = true;
        if (isConfigured) {
          registerModule();
          hardware->updateLCD("Connected", "System Ready");
        } else {
          hardware->updateLCD("Connected", "Register device");
        }
        break;
        
      case WebsocketsEvent::ConnectionClosed:
        Serial.println("WebSocket Disconnected from server");
        isConnected = false;
        if (isConfigured) {
          hardware->updateLCD("Disconnected", "Reconnecting...");
        }
        break;
        
      case WebsocketsEvent::GotPing:
        webSocket->pong();
        break;
        
      default:
        break;
    } });

  // Attempt initial connection
  return reconnect();
}

bool ServerManager::reconnect()
{
  if (isConnected)
    return true;

  unsigned long currentTime = millis();
  if (currentTime - lastReconnectAttempt < 5000)
  {
    return false; // Don't try to reconnect too frequently
  }

  lastReconnectAttempt = currentTime;

  Serial.println("Attempting to connect to: " + serverURL);

  if (webSocket->connect(serverURL))
  {
    Serial.println("WebSocket connection successful");
    return true;
  }
  else
  {
    Serial.println("WebSocket connection failed");
    return false;
  }
}

void ServerManager::loop()
{
  if (!isConnected)
  {
    reconnect();
    return;
  }

  webSocket->poll();

  unsigned long currentTime = millis();

  if (isConfigured && currentTime - lastPing >= PING_INTERVAL)
  {
    sendPing();
    lastPing = currentTime;
  }
}

void ServerManager::handleMessage(const String &message)
{
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, message);

  if (error)
  {
    Serial.println("Failed to parse WebSocket message: " + String(error.c_str()));
    return;
  }

  String messageType = doc["type"].as<String>();

  if (messageType == "connected")
  {
    Serial.println("Server acknowledged connection");
  }
  else if (messageType == "registered")
  {
    Serial.println("Module registered successfully");
    hardware->updateLCD("Registered", "System Ready");
  }
  else if (messageType == "pong")
  {
    // Server responded to our ping
  }
  else if (messageType == "lock" || messageType == "unlock")
  {
    handleLockUnlockCommand(doc);
  }
  else if (messageType == "module_configured")
  {
    handleModuleConfiguration(doc);
  }
  else
  {
    Serial.println("Unknown message type: " + messageType);
  }
}

void ServerManager::registerModule()
{
  if (!isConfigured || !isConnected)
    return;

  DynamicJsonDocument doc(256);
  doc["type"] = "register";
  doc["moduleId"] = moduleId;

  String message;
  serializeJson(doc, message);

  webSocket->send(message);
  Serial.println("Sent registration for module: " + moduleId);
}

void ServerManager::handleLockUnlockCommand(const JsonDocument &doc)
{
  String lockerId = doc["lockerId"].as<String>();
  String action = doc["type"].as<String>(); // "lock" or "unlock"

  Serial.println("Received " + action + " command for locker: " + lockerId);

  if (action == "unlock")
  {
    hardware->unlockLocker(lockerId);
    // Send status update after unlocking
    sendStatusUpdate(lockerId, "unlocked");
  }
  else if (action == "lock")
  {
    hardware->lockLocker(lockerId);
    // Send status update after locking
    sendStatusUpdate(lockerId, "locked");
  }
}

void ServerManager::sendStatusUpdate(const String &lockerId, const String &status)
{
  if (!isConfigured || !isConnected)
    return;

  DynamicJsonDocument doc(512);
  doc["type"] = "status_update";
  doc["moduleId"] = moduleId;
  doc["lockerId"] = lockerId;
  doc["status"] = status;
  doc["timestamp"] = millis();

  String message;
  serializeJson(doc, message);

  webSocket->send(message);
  Serial.println("Sent status update - Locker " + lockerId + ": " + status);
}

void ServerManager::sendLockerStatus(const String &lockerId, bool isOccupied)
{
  if (!isConfigured || !isConnected)
    return;

  DynamicJsonDocument doc(512);
  doc["type"] = "locker_status";
  doc["moduleId"] = moduleId;
  doc["lockerId"] = lockerId;
  doc["occupied"] = isOccupied;
  doc["timestamp"] = millis();

  String message;
  serializeJson(doc, message);

  webSocket->send(message);
  Serial.printf("Status sent: Locker %s - %s\n",
                lockerId.c_str(), isOccupied ? "Occupied" : "Empty");
}

void ServerManager::sendPing()
{
  if (!isConfigured || !isConnected)
    return;

  DynamicJsonDocument doc(256);
  doc["type"] = "ping";
  doc["moduleId"] = moduleId;

  String message;
  serializeJson(doc, message);

  webSocket->send(message);
}

void ServerManager::handleModuleConfiguration(const JsonDocument &doc)
{
  String configModuleId = doc["moduleId"];
  JsonArrayConst lockerArray = doc["lockerIds"];

  // Convert JsonArray to String array
  String *lockerIds = new String[lockerArray.size()];
  for (int i = 0; i < lockerArray.size(); i++)
  {
    lockerIds[i] = lockerArray[i].as<String>();
  }

  hardware->saveLockerConfiguration(configModuleId, lockerIds, lockerArray.size());

  delete[] lockerIds;

  Serial.println("Module configured with ID: " + configModuleId);
  hardware->updateLCD("Configured!", "Restarting...");

  delay(2000);
  ESP.restart();
}
}

void ServerManager::handleModuleConfiguration(const JsonDocument &doc)
{
  String configModuleId = doc["moduleId"];
  JsonArrayConst lockerArray = doc["lockerIds"];

  // Convert JsonArray to String array
  String *lockerIds = new String[lockerArray.size()];
  for (int i = 0; i < lockerArray.size(); i++)
  {
    lockerIds[i] = lockerArray[i].as<String>();
  }

  hardware->saveLockerConfiguration(configModuleId, lockerIds, lockerArray.size());

  delete[] lockerIds;

  Serial.println("Module configured with ID: " + configModuleId);
  hardware->updateLCD("Configured!", "Restarting...");

  delay(2000);
  ESP.restart();
}

void ServerManager::handleUnlockEvent(const JsonDocument &doc)
{
  String lockerId = doc["lockerId"];
  String action = doc["action"];

  if (action == "unlock")
  {
    hardware->unlockLocker(lockerId);
  }
}

void ServerManager::handleNFCValidationResult(const JsonDocument &doc)
{
  if (!hardware->isWaitingForNFCValidation())
    return;

  bool valid = doc["valid"];
  String lockerId = doc["lockerId"];
  String message = doc["message"];

  if (valid)
  {
    hardware->toggleLocker(lockerId);
    hardware->setNFCValidationResult(true, "Locker " + lockerId);
  }
  else
  {
    hardware->setNFCValidationResult(false, message.length() > 0 ? message : "Invalid NFC");
  }
}

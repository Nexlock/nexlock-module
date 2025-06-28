#include "server_manager.h"
#include "hardware_manager.h"

ServerManager *ServerManager::instance = nullptr;

ServerManager::ServerManager(HardwareManager *hw, const String &mac)
    : hardware(hw), macAddress(mac), isConnected(false), isConfigured(false),
      lastPing(0), lastReconnectAttempt(0), lastAvailableBroadcast(0)
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

  // Send available module broadcast if not configured
  if (!isConfigured && currentTime - lastAvailableBroadcast >= AVAILABLE_BROADCAST_INTERVAL)
  {
    sendAvailableModuleBroadcast();
    lastAvailableBroadcast = currentTime;
  }
}

void ServerManager::sendAvailableModuleBroadcast()
{
  if (isConfigured || !isConnected)
    return;

  StaticJsonDocument<MEDIUM_JSON_SIZE> doc;
  doc["type"] = "module_available";
  doc["macAddress"] = macAddress;
  doc["deviceInfo"] = String(DEVICE_NAME) + " v" + String(FIRMWARE_VERSION);
  doc["version"] = FIRMWARE_VERSION;
  doc["capabilities"] = MAX_LOCKERS;
  doc["timestamp"] = millis();

  String message;
  message.reserve(300);
  serializeJson(doc, message);

  webSocket->send(message);
  Serial.println(F("Sent available module broadcast"));
}

void ServerManager::handleMessage(const String &message)
{
  StaticJsonDocument<LARGE_JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error)
  {
    Serial.print(F("Parse error: "));
    Serial.println(error.c_str());
    return;
  }

  const char *messageType = doc["type"];

  if (strcmp(messageType, "connected") == 0)
  {
    Serial.println(F("Server acknowledged connection"));
  }
  else if (strcmp(messageType, "registered") == 0)
  {
    Serial.println(F("Module registered successfully"));
    hardware->updateLCD(F("Registered"), F("System Ready"));
  }
  else if (strcmp(messageType, "pong") == 0)
  {
    // Server responded to our ping
  }
  else if (strcmp(messageType, "lock") == 0 || strcmp(messageType, "unlock") == 0)
  {
    handleLockUnlockCommand(doc);
  }
  else if (strcmp(messageType, "module_configured") == 0)
  {
    handleModuleConfiguration(doc);
  }
}

void ServerManager::registerModule()
{
  if (!isConfigured || !isConnected)
    return;

  StaticJsonDocument<SMALL_JSON_SIZE> doc;
  doc["type"] = "register";
  doc["moduleId"] = moduleId;

  String message;
  message.reserve(150);
  serializeJson(doc, message);

  webSocket->send(message);
  Serial.print(F("Registered module: "));
  Serial.println(moduleId);
}

void ServerManager::handleLockUnlockCommand(const JsonDocument &doc)
{
  String lockerId = doc["lockerId"].as<String>();
  const char *action = doc["type"];

  Serial.print(F("Command "));
  Serial.print(action);
  Serial.print(F(" for locker: "));
  Serial.println(lockerId);

  if (strcmp(action, "unlock") == 0)
  {
    hardware->unlockLocker(lockerId);
    sendStatusUpdate(lockerId, "unlocked");
  }
  else if (strcmp(action, "lock") == 0)
  {
    hardware->lockLocker(lockerId);
    sendStatusUpdate(lockerId, "locked");
  }
}

void ServerManager::sendStatusUpdate(const String &lockerId, const String &status)
{
  if (!isConfigured || !isConnected)
    return;

  StaticJsonDocument<MEDIUM_JSON_SIZE> doc;
  doc["type"] = "status_update";
  doc["moduleId"] = moduleId;
  doc["lockerId"] = lockerId;
  doc["status"] = status;
  doc["timestamp"] = millis();

  String message;
  message.reserve(200);
  serializeJson(doc, message);

  webSocket->send(message);
}

void ServerManager::sendPing()
{
  if (!isConfigured || !isConnected)
    return;

  StaticJsonDocument<SMALL_JSON_SIZE> doc;
  doc["type"] = "ping";
  doc["moduleId"] = moduleId;

  String message;
  message.reserve(100);
  serializeJson(doc, message);

  webSocket->send(message);
}

void ServerManager::handleModuleConfiguration(const JsonDocument &doc)
{
  String configModuleId = doc["moduleId"];
  JsonArrayConst lockerArray = doc["lockerIds"];

  String *lockerIds = new String[lockerArray.size()];
  for (size_t i = 0; i < lockerArray.size(); i++)
  {
    lockerIds[i] = lockerArray[i].as<String>();
  }

  hardware->saveLockerConfiguration(configModuleId, lockerIds, lockerArray.size());

  delete[] lockerIds;

  Serial.print(F("Module configured: "));
  Serial.println(configModuleId);
  hardware->updateLCD(F("Configured!"), F("Restarting..."));

  delay(2000);
  ESP.restart();
}
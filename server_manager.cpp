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

  Serial.print(F("ServerManager initialize: moduleId="));
  Serial.print(moduleId);
  Serial.print(F(", isConfigured="));
  Serial.println(isConfigured);

  // Construct WebSocket URL for raw WebSocket connection (not Socket.IO)
  serverURL = "ws://" + serverIP + ":" + String(serverPort) + "/ws";

  // Set proper headers to identify as ESP32 module
  webSocket->addHeader("User-Agent", "ESP32-NexLock/1.0.0");

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
          Serial.println("Module is configured, registering...");
          registerModule();
          hardware->updateLCD("Connected", "System Ready");
        } else {
          Serial.println("Module not configured, will broadcast availability");
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

  // Check configuration status from hardware manager and sync
  bool hardwareConfigured = hardware->getConfigurationStatus();
  if (!isConfigured && hardwareConfigured)
  {
    Serial.println(F("Configuration status changed - now configured"));
    isConfigured = true;
    moduleId = hardware->getModuleId();
    registerModule();
    return;
  }

  if (isConfigured && currentTime - lastPing >= PING_INTERVAL)
  {
    sendPing();
    lastPing = currentTime;
  }

  // Send periodic status updates for all lockers
  if (isConfigured && currentTime - lastStatusUpdate >= STATUS_CHECK_INTERVAL)
  {
    sendAllLockerStatusUpdates();
    lastStatusUpdate = currentTime;
  }

  // Send available module broadcast ONLY if not configured
  if (!isConfigured && !hardwareConfigured && currentTime - lastAvailableBroadcast >= AVAILABLE_BROADCAST_INTERVAL)
  {
    sendAvailableModuleBroadcast();
    lastAvailableBroadcast = currentTime;
  }
}

void ServerManager::sendAvailableModuleBroadcast()
{
  // Triple-check: don't broadcast if already configured
  if (isConfigured || !isConnected || hardware->getConfigurationStatus())
  {
    Serial.println(F("Skipping availability broadcast - module is configured"));
    return;
  }

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

  bool success = false;
  if (strcmp(action, "unlock") == 0)
  {
    success = hardware->unlockLocker(lockerId);
    if (success)
    {
      sendStatusUpdate(lockerId, "unlocked");
    }
  }
  else if (strcmp(action, "lock") == 0)
  {
    success = hardware->lockLocker(lockerId);
    if (success)
    {
      sendStatusUpdate(lockerId, "locked");
    }
  }

  if (!success)
  {
    Serial.print(F("Failed to execute command for locker: "));
    Serial.println(lockerId);
    sendStatusUpdate(lockerId, "error");
  }
}

void ServerManager::sendAllLockerStatusUpdates()
{
  if (!isConfigured || !isConnected)
    return;

  LockerConfig *lockers = hardware->getLockers();
  int numLockers = hardware->getNumLockers();

  for (int i = 0; i < numLockers; i++)
  {
    String status = hardware->getLockerStatus(lockers[i].lockerId);
    sendStatusUpdate(lockers[i].lockerId, status);
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
  String configMacAddress = doc["macAddress"];
  JsonArrayConst lockerArray = doc["lockerIds"];

  Serial.print(F("Received module configuration: moduleId="));
  Serial.println(configModuleId);
  Serial.print(F("Expected MAC address: "));
  Serial.println(configMacAddress);
  Serial.print(F("Our MAC address: "));
  Serial.println(macAddress);

  // First verify this configuration is meant for us by checking MAC address
  if (configMacAddress != macAddress)
  {
    Serial.println(F("ERROR: MAC address mismatch - ignoring configuration"));
    Serial.print(F("Expected: "));
    Serial.println(configMacAddress);
    Serial.print(F("Actual: "));
    Serial.println(macAddress);

    // Send error response
    if (webSocket && isConnected)
    {
      StaticJsonDocument<MEDIUM_JSON_SIZE> errorDoc;
      errorDoc["type"] = "configuration_error";
      errorDoc["error"] = "MAC address mismatch";
      errorDoc["expectedMac"] = configMacAddress;
      errorDoc["actualMac"] = macAddress;

      String errorMessage;
      errorMessage.reserve(200);
      serializeJson(errorDoc, errorMessage);
      webSocket->send(errorMessage);
    }
    return;
  }

  Serial.println(F("MAC address verified - proceeding with configuration"));

  String *lockerIds = new String[lockerArray.size()];
  for (size_t i = 0; i < lockerArray.size(); i++)
  {
    lockerIds[i] = lockerArray[i].as<String>();
    Serial.print(F("Locker ID: "));
    Serial.println(lockerIds[i]);
  }

  // Save configuration to persistent storage
  hardware->saveLockerConfiguration(configModuleId, lockerIds, lockerArray.size());

  delete[] lockerIds;

  // Update local state immediately
  isConfigured = true;
  moduleId = configModuleId;

  Serial.print(F("Module configured successfully: "));
  Serial.println(configModuleId);
  Serial.println(F("Will restart in 3 seconds..."));

  // Send success confirmation
  if (webSocket && isConnected)
  {
    StaticJsonDocument<MEDIUM_JSON_SIZE> confirmDoc;
    confirmDoc["type"] = "configuration_success";
    confirmDoc["moduleId"] = configModuleId;
    confirmDoc["macAddress"] = macAddress;

    String confirmMessage;
    confirmMessage.reserve(200);
    serializeJson(confirmDoc, confirmMessage);
    webSocket->send(confirmMessage);
  }

  hardware->updateLCD(F("Configured!"), F("Restarting..."));

  delay(3000);
  ESP.restart();
}
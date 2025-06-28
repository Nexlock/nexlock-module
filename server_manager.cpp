#include "server_manager.h"
#include "hardware_manager.h"

ServerManager *ServerManager::instance = nullptr;

ServerManager::ServerManager(HardwareManager *hw, const String &mac)
    : hardware(hw), macAddress(mac), isConnected(false), isConfigured(false),
      lastPing(0), lastAvailableBroadcast(0), lastReconnectAttempt(0)
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

  // Construct WebSocket URL (assuming Socket.IO endpoint)
  serverURL = "ws://" + serverIP + ":" + String(serverPort) + "/socket.io/?EIO=4&transport=websocket";

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
          hardware->updateLCD("Connected", "Awaiting config");
          broadcastAvailable();
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

  if (isConfigured)
  {
    if (currentTime - lastPing >= PING_INTERVAL)
    {
      sendPing();
      lastPing = currentTime;
    }
  }
  else
  {
    if (currentTime - lastAvailableBroadcast >= AVAILABLE_BROADCAST_INTERVAL)
    {
      broadcastAvailable();
      lastAvailableBroadcast = currentTime;
    }
  }
}

void ServerManager::sendSocketIOMessage(const String &event, const String &data)
{
  if (!isConnected)
    return;

  // Socket.IO protocol: 42["event","data"]
  String message = "42[\"" + event + "\"," + data + "]";
  webSocket->send(message);
}

void ServerManager::handleMessage(const String &message)
{
  // Parse Socket.IO message format
  if (message.startsWith("42["))
  {
    // Extract JSON array from Socket.IO message
    int startIndex = message.indexOf('[');
    int endIndex = message.lastIndexOf(']');

    if (startIndex >= 0 && endIndex > startIndex)
    {
      String jsonStr = message.substring(startIndex, endIndex + 1);

      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, jsonStr);

      if (!error && doc.is<JsonArray>() && doc.size() >= 1)
      {
        String event = doc[0].as<String>();

        if (event == "module-configured" && doc.size() >= 2)
        {
          handleModuleConfiguration(doc[1]);
        }
        else if (event == "unlock" && doc.size() >= 2)
        {
          handleUnlockEvent(doc[1]);
        }
        else if (event == "nfc-validation-result" && doc.size() >= 2)
        {
          handleNFCValidationResult(doc[1]);
        }
        else if (event == "registered")
        {
          Serial.println("Module registered successfully");
        }
      }
    }
  }
  else if (message == "0")
  {
    // Socket.IO connection acknowledgment
    Serial.println("Socket.IO connection established");
  }
  else if (message.startsWith("40"))
  {
    // Socket.IO connection to namespace
    Serial.println("Socket.IO namespace connected");
  }
}

void ServerManager::broadcastAvailable()
{
  if (!isConnected)
    return;

  DynamicJsonDocument doc(512);
  doc["macAddress"] = macAddress;
  doc["deviceInfo"] = DEVICE_NAME " Module";
  doc["version"] = FIRMWARE_VERSION;
  doc["capabilities"] = MAX_LOCKERS;

  String payload;
  serializeJson(doc, payload);

  sendSocketIOMessage("module-available", payload);
  Serial.println("Broadcasting available status");
}

void ServerManager::registerModule()
{
  if (!isConfigured)
    return;

  DynamicJsonDocument doc(256);
  doc["moduleId"] = moduleId;

  String payload;
  serializeJson(doc, payload);

  sendSocketIOMessage("register", payload);
}

void ServerManager::validateNFC(const String &nfcCode)
{
  if (!isConfigured)
    return;

  DynamicJsonDocument doc(1024);
  doc["nfcCode"] = nfcCode;
  doc["moduleId"] = moduleId;

  String payload;
  serializeJson(doc, payload);

  sendSocketIOMessage("validate-nfc", payload);
}

void ServerManager::sendLockerStatus(const String &lockerId, bool isOccupied)
{
  if (!isConfigured)
    return;

  DynamicJsonDocument doc(512);
  doc["moduleId"] = moduleId;
  doc["lockerId"] = lockerId;
  doc["occupied"] = isOccupied;
  doc["timestamp"] = millis();

  String payload;
  serializeJson(doc, payload);

  sendSocketIOMessage("locker-status", payload);

  Serial.printf("Status sent: Locker %s - %s\n",
                lockerId.c_str(), isOccupied ? "Occupied" : "Empty");
}

void ServerManager::sendPing()
{
  if (!isConfigured)
    return;

  DynamicJsonDocument doc(256);
  doc["moduleId"] = moduleId;

  String payload;
  serializeJson(doc, payload);

  sendSocketIOMessage("ping", payload);
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

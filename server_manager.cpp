#include "server_manager.h"

ServerManager* ServerManager::instance = nullptr;

ServerManager::ServerManager(HardwareManager* hw, const String& mac) 
  : hardware(hw), macAddress(mac), isConnected(false), isConfigured(false),
    lastPing(0), lastAvailableBroadcast(0) {
  
  socketIO = new SocketIOclient();
  instance = this;
}

ServerManager::~ServerManager() {
  delete socketIO;
  instance = nullptr;
}

bool ServerManager::initialize(const String& serverIP, int serverPort) {
  moduleId = hardware->getModuleId();
  isConfigured = hardware->getConfigurationStatus();
  
  socketIO->begin(serverIP.c_str(), serverPort, "/socket.io/?EIO=4");
  socketIO->onEvent(socketIOEventWrapper);
  
  return true;
}

void ServerManager::loop() {
  socketIO->loop();
  
  unsigned long currentTime = millis();
  
  if (isConfigured) {
    if (currentTime - lastPing >= PING_INTERVAL) {
      sendPing();
      lastPing = currentTime;
    }
  } else {
    if (currentTime - lastAvailableBroadcast >= AVAILABLE_BROADCAST_INTERVAL) {
      broadcastAvailable();
      lastAvailableBroadcast = currentTime;
    }
  }
}

void ServerManager::broadcastAvailable() {
  if (!socketIO->isConnected()) return;
  
  DynamicJsonDocument doc(512);
  doc["macAddress"] = macAddress;
  doc["deviceInfo"] = DEVICE_NAME " Module";
  doc["version"] = FIRMWARE_VERSION;
  doc["capabilities"] = MAX_LOCKERS;
  
  String payload;
  serializeJson(doc, payload);
  
  socketIO->emit("module-available", payload.c_str());
  Serial.println("Broadcasting available status");
}

void ServerManager::registerModule() {
  if (!isConfigured) return;
  
  DynamicJsonDocument doc(256);
  doc["moduleId"] = moduleId;
  
  String payload;
  serializeJson(doc, payload);
  
  socketIO->emit("register", payload.c_str());
}

void ServerManager::validateNFC(const String& nfcCode) {
  if (!isConfigured) return;
  
  DynamicJsonDocument doc(1024);
  doc["nfcCode"] = nfcCode;
  doc["moduleId"] = moduleId;
  
  String payload;
  serializeJson(doc, payload);
  
  socketIO->emit("validate-nfc", payload.c_str());
}

void ServerManager::sendLockerStatus(const String& lockerId, bool isOccupied) {
  if (!isConfigured) return;
  
  DynamicJsonDocument doc(512);
  doc["moduleId"] = moduleId;
  doc["lockerId"] = lockerId;
  doc["occupied"] = isOccupied;
  doc["timestamp"] = millis();
  
  String payload;
  serializeJson(doc, payload);
  
  socketIO->emit("locker-status", payload.c_str());
  
  Serial.printf("Status sent: Locker %s - %s\n", 
               lockerId.c_str(), isOccupied ? "Occupied" : "Empty");
}

void ServerManager::sendPing() {
  if (!isConfigured) return;
  
  DynamicJsonDocument doc(256);
  doc["moduleId"] = moduleId;
  
  String payload;
  serializeJson(doc, payload);
  
  socketIO->emit("ping", payload.c_str());
}

void ServerManager::socketIOEventWrapper(socketIOmessageType_t type, uint8_t* payload, size_t length) {
  if (instance) {
    instance->socketIOEvent(type, payload, length);
  }
}

void ServerManager::socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
  switch(type) {
    case sIOtype_DISCONNECT:
      Serial.println("Disconnected from server");
      isConnected = false;
      if (isConfigured) {
        hardware->updateLCD("Disconnected", "Reconnecting...");
      }
      break;
      
    case sIOtype_CONNECT:
      Serial.println("Connected to server");
      isConnected = true;
      if (isConfigured) {
        registerModule();
        hardware->updateLCD("Connected", "System Ready");
      } else {
        hardware->updateLCD("Connected", "Awaiting config");
        broadcastAvailable();
      }
      break;
      
    case sIOtype_EVENT: {
      String eventName = (char*)payload;
      
      if (eventName.startsWith("module-configured")) {
        handleModuleConfiguration(payload, length);
      } else if (eventName.startsWith("unlock")) {
        handleUnlockEvent(payload, length);
      } else if (eventName.startsWith("nfc-validation-result")) {
        handleNFCValidationResult(payload, length);
      } else if (eventName.startsWith("registered")) {
        Serial.println("Module registered successfully");
      }
      break;
    }
    
    default:
      break;
  }
}

void ServerManager::handleModuleConfiguration(uint8_t* payload, size_t length) {
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, payload, length);
  
  String configModuleId = doc["moduleId"];
  JsonArray lockerArray = doc["lockerIds"];
  
  // Convert JsonArray to String array
  String* lockerIds = new String[lockerArray.size()];
  for (int i = 0; i < lockerArray.size(); i++) {
    lockerIds[i] = lockerArray[i].as<String>();
  }
  
  hardware->saveLockerConfiguration(configModuleId, lockerIds, lockerArray.size());
  
  delete[] lockerIds;
  
  Serial.println("Module configured with ID: " + configModuleId);
  hardware->updateLCD("Configured!", "Restarting...");
  
  delay(2000);
  ESP.restart();
}

void ServerManager::handleUnlockEvent(uint8_t* payload, size_t length) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload, length);
  
  String lockerId = doc["lockerId"];
  String action = doc["action"];
  
  if (action == "unlock") {
    hardware->unlockLocker(lockerId);
  }
}

void ServerManager::handleNFCValidationResult(uint8_t* payload, size_t length) {
  if (!hardware->isWaitingForNFCValidation()) return;
  
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload, length);
  
  bool valid = doc["valid"];
  String lockerId = doc["lockerId"];
  String message = doc["message"];
  
  if (valid) {
    hardware->toggleLocker(lockerId);
    hardware->setNFCValidationResult(true, "Locker " + lockerId);
  } else {
    hardware->setNFCValidationResult(false, message);
  }
}

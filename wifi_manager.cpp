#include "wifi_manager.h"
#include <esp_wifi.h>

WiFiManager::WiFiManager(Preferences* prefs) : preferences(prefs), isProvisioned(false), serverPort(DEFAULT_SERVER_PORT) {
  provisioningServer = new WebServer(80);
  generateMacAddress();
}

WiFiManager::~WiFiManager() {
  delete provisioningServer;
}

void WiFiManager::generateMacAddress() {
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  macAddress = "";
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) macAddress += "0";
    macAddress += String(mac[i], HEX);
  }
  macAddress.toUpperCase();
}

bool WiFiManager::initialize() {
  loadConfiguration();
  
  if (!isProvisioned) {
    startProvisioningMode();
    return false;
  }
  
  return connectToWiFi();
}

void WiFiManager::loadConfiguration() {
  ssid = preferences->getString("ssid", "");
  password = preferences->getString("password", "");
  serverIP = preferences->getString("serverIP", "");
  serverPort = preferences->getInt("serverPort", DEFAULT_SERVER_PORT);
  
  isProvisioned = (ssid.length() > 0 && password.length() > 0 && serverIP.length() > 0);
}

void WiFiManager::startProvisioningMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("NexLock_" + macAddress, "12345678");
  
  setupProvisioningServer();
  provisioningServer->begin();
  
  Serial.println("Provisioning mode started");
  Serial.println("AP: NexLock_" + macAddress);
  Serial.println("IP: " + WiFi.softAPIP().toString());
}

void WiFiManager::setupProvisioningServer() {
  provisioningServer->on("/", [this]() {
    String html = R"(<!DOCTYPE html><html><head><title>NexLock WiFi Setup</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body { font-family: Arial; margin: 40px; background: #f0f0f0; }.container { background: white; padding: 20px; border-radius: 10px; }input { width: 100%; padding: 10px; margin: 10px 0; }button { background: #007bff; color: white; padding: 15px; border: none; border-radius: 5px; width: 100%; }</style></head><body><div class="container"><h2>NexLock WiFi Configuration</h2><form action="/configure" method="POST"><label>WiFi Network:</label><input type="text" name="ssid" placeholder="Enter WiFi SSID" required><label>WiFi Password:</label><input type="password" name="password" placeholder="Enter WiFi Password" required><label>Server IP:</label><input type="text" name="serverIP" placeholder="e.g., 192.168.1.100" required><label>Server Port:</label><input type="number" name="serverPort" value="3000" required><button type="submit">Configure WiFi</button></form><p><strong>Device ID:</strong> )" + macAddress + R"(</p><p><small>Use this ID when configuring in the admin panel</small></p></div></body></html>)";
    provisioningServer->send(200, "text/html", html);
  });
  
  provisioningServer->on("/configure", HTTP_POST, [this]() {
    saveWiFiConfig(provisioningServer->arg("ssid"), provisioningServer->arg("password"),
                   provisioningServer->arg("serverIP"), provisioningServer->arg("serverPort").toInt());
    
    provisioningServer->send(200, "text/html", 
      "<h2>Configuration Saved!</h2><p>Device will restart and connect to WiFi.</p>");
    
    delay(2000);
    ESP.restart();
  });
}

bool WiFiManager::connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECTION_TIMEOUT) {
    delay(1000);
    attempts++;
    Serial.println("Connecting to WiFi... " + String(attempts));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    return true;
  }
  
  Serial.println("WiFi connection failed");
  return false;
}

void WiFiManager::handleProvisioning() {
  provisioningServer->handleClient();
}

bool WiFiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::getProvisioningStatus() const {
  return isProvisioned;
}

void WiFiManager::saveWiFiConfig(const String& ssid, const String& password, 
                                 const String& serverIP, int serverPort) {
  preferences->putString("ssid", ssid);
  preferences->putString("password", password);
  preferences->putString("serverIP", serverIP);
  preferences->putInt("serverPort", serverPort);
  
  this->ssid = ssid;
  this->password = password;
  this->serverIP = serverIP;
  this->serverPort = serverPort;
  this->isProvisioned = true;
}

void WiFiManager::factoryReset() {
  preferences->clear();
  ESP.restart();
}

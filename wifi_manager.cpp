#include "wifi_manager.h"
#include <esp_wifi.h>

WiFiManager::WiFiManager(Preferences *prefs) : preferences(prefs), isProvisioned(false), serverPort(DEFAULT_SERVER_PORT), useESPProvisioning(false)
{
  provisioningServer = new WebServer(80);
  generateMacAddress();
}

WiFiManager::~WiFiManager()
{
  delete provisioningServer;
}

void WiFiManager::generateMacAddress()
{
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  macAddress = "";
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] < 0x10)
      macAddress += "0";
    macAddress += String(mac[i], HEX);
  }
  macAddress.toUpperCase();
}

bool WiFiManager::initialize()
{
  loadConfiguration();

  if (!isProvisioned)
  {
    startProvisioningMode();
    return false;
  }

  return connectToWiFi();
}

void WiFiManager::loadConfiguration()
{
  ssid = preferences->getString("ssid", "");
  password = preferences->getString("password", "");
  serverIP = preferences->getString("serverIP", "");
  serverPort = preferences->getInt("serverPort", DEFAULT_SERVER_PORT);

  isProvisioned = (ssid.length() > 0 && password.length() > 0 && serverIP.length() > 0);
}

void WiFiManager::startProvisioningMode(bool preferESP)
{
  useESPProvisioning = preferESP;

  if (useESPProvisioning)
  {
    startESPProvisioning();
  }
  else
  {
    // Fallback to HTML provisioning
    WiFi.mode(WIFI_AP);
    WiFi.softAP("NexLock_" + macAddress, "12345678");

    setupProvisioningServer();
    provisioningServer->begin();

    Serial.println("HTML Provisioning mode started");
    Serial.println("AP: NexLock_" + macAddress);
    Serial.println("IP: " + WiFi.softAPIP().toString());
  }
}

void WiFiManager::startESPProvisioning()
{
  WiFi.mode(WIFI_AP_STA);

  // Set device name for ESP provisioning
  String deviceName = "NexLock_" + macAddress;

  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM,
                          NETWORK_PROV_SECURITY_1, "nexlock123", deviceName.c_str());

  // Note: ESP32 WiFiProv library doesn't have onProvisioningComplete callback
  // We'll handle completion through WiFi events instead

  Serial.println("ESP Provisioning started");
  Serial.println("Device: " + deviceName);
  Serial.println("Use NexLock mobile app to provision");
}

void WiFiManager::setupProvisioningServer()
{
  provisioningServer->on("/", [this]()
                         {
    String html;
    html.reserve(800); // Pre-allocate memory
    html += FPSTR(HTML_HEADER);
    html += FPSTR(HTML_FORM);
    html += "<p><strong>ID:</strong> " + macAddress + "</p>";
    html += FPSTR(HTML_FOOTER);
    provisioningServer->send(200, "text/html", html); });

  provisioningServer->on("/configure", HTTP_POST, [this]()
                         {
    saveWiFiConfig(provisioningServer->arg("ssid"), provisioningServer->arg("password"),
                   provisioningServer->arg("serverIP"), provisioningServer->arg("serverPort").toInt());
    
    provisioningServer->send(200, "text/html", 
      "<h2>Saved!</h2><p>Restarting...</p>");
    
    delay(1000);
    ESP.restart(); });
}

bool WiFiManager::connectToWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECTION_TIMEOUT)
  {
    delay(1000);
    attempts++;
    Serial.println("Connecting to WiFi... " + String(attempts));
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    return true;
  }

  Serial.println("WiFi connection failed");
  return false;
}

void WiFiManager::handleProvisioning()
{
  if (useESPProvisioning)
  {
    // ESP provisioning is handled automatically
    return;
  }

  // Handle HTML provisioning
  provisioningServer->handleClient();
}

bool WiFiManager::isConnected() const
{
  return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::getProvisioningStatus() const
{
  return isProvisioned;
}

void WiFiManager::saveWiFiConfig(const String &ssid, const String &password,
                                 const String &serverIP, int serverPort)
{
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

void WiFiManager::factoryReset()
{
  preferences->clear();
  ESP.restart();
}

void WiFiManager::provisioningHandler(arduino_event_t *sys_event)
{
  switch (sys_event->event_id)
  {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial.println("Disconnected. Reconnecting...");
    break;
  default:
    break;
  }
}
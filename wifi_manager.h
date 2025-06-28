#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "WiFiProv.h"
#include "config.h"

class WiFiManager
{
private:
  WebServer *provisioningServer;
  Preferences *preferences;
  String macAddress;
  String ssid;
  String password;
  String serverIP;
  int serverPort;
  bool isProvisioned;
  bool useESPProvisioning;

  void setupProvisioningServer();
  void generateMacAddress();
  void startESPProvisioning();
  static void provisioningHandler(arduino_event_t *sys_event);

public:
  WiFiManager(Preferences *prefs);
  ~WiFiManager();

  bool initialize();
  void startProvisioningMode(bool preferESP = true);
  bool connectToWiFi();
  void handleProvisioning();
  bool isConnected() const;
  bool getProvisioningStatus() const;

  // Getters
  String getMacAddress() const { return macAddress; }
  String getServerIP() const { return serverIP; }
  int getServerPort() const { return serverPort; }

  // Configuration
  void saveWiFiConfig(const String &ssid, const String &password,
                      const String &serverIP, int serverPort);
  void loadConfiguration();
  void factoryReset();
};

#endif

#include <Preferences.h>
#include "config.h"
#include "wifi_manager.h"
#include "hardware_manager.h"
#include "server_manager.h"

// Global objects
Preferences preferences;
WiFiManager *wifiManager = nullptr;
HardwareManager *hardwareManager = nullptr;
ServerManager *serverManager = nullptr;

// Timing variables
unsigned long lastStatusCheck = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("=================================");
  Serial.println("NexLock v" FIRMWARE_VERSION " starting...");
  Serial.println("=================================");

  // Initialize preferences
  preferences.begin("nexlock", false);

  // Initialize managers in order
  initializeManagers();

  Serial.println("System initialization complete");
}

void loop()
{
  // Handle factory reset request (highest priority)
  if (hardwareManager && hardwareManager->checkConfigButton())
  {
    performFactoryReset();
    return;
  }

  // Handle WiFi provisioning if not configured
  if (wifiManager && !wifiManager->getProvisioningStatus())
  {
    wifiManager->handleProvisioning();
    return;
  }

  // Check WiFi connection status
  if (wifiManager && !wifiManager->isConnected())
  {
    handleWiFiDisconnection();
    return;
  }

  // Main application loop
  runMainLoop();

  delay(100); // Small delay to prevent excessive CPU usage
}

void initializeManagers()
{
  // Initialize hardware manager first
  hardwareManager = new HardwareManager(&preferences);
  if (!hardwareManager)
  {
    Serial.println("ERROR: Failed to create HardwareManager");
    return;
  }

  bool hardwareReady = hardwareManager->initialize();
  Serial.println("Hardware initialization: " + String(hardwareReady ? "SUCCESS" : "PENDING"));

  // Initialize WiFi manager
  wifiManager = new WiFiManager(&preferences);
  if (!wifiManager)
  {
    Serial.println("ERROR: Failed to create WiFiManager");
    return;
  }

  bool wifiReady = wifiManager->initialize();
  Serial.println("WiFi initialization: " + String(wifiReady ? "SUCCESS" : "PROVISIONING"));

  // Initialize server manager if WiFi is ready
  if (wifiReady)
  {
    serverManager = new ServerManager(hardwareManager, wifiManager->getMacAddress());
    if (serverManager)
    {
      bool serverReady = serverManager->initialize(wifiManager->getServerIP(), wifiManager->getServerPort());
      Serial.println("Server initialization: " + String(serverReady ? "SUCCESS" : "FAILED"));
    }
  }
}

void runMainLoop()
{
  // Handle server communication
  if (serverManager)
  {
    serverManager->loop();
  }

  // Handle manual operations (for testing/maintenance)
  handleManualOperations();

  // Check locker statuses periodically
  handleLockerStatusCheck();
}

void handleManualOperations()
{
  if (!hardwareManager)
    return;

  // Check for manual lock/unlock operations via NFC or other inputs
  // This can be used for maintenance or testing
  String nfcCode;
  if (hardwareManager->scanNFC(nfcCode))
  {
    Serial.println("NFC scanned: " + nfcCode);

    if (hardwareManager->getConfigurationStatus())
    {
      // For now, we'll just show a message since the new flow doesn't use NFC validation
      hardwareManager->updateLCD("NFC Detected", nfcCode.substring(0, 16));
      delay(2000);
      hardwareManager->updateSystemStatus();
    }
    else
    {
      hardwareManager->updateLCD("Not Configured", "Contact admin");
      delay(2000);
      hardwareManager->updateSystemStatus();
    }
  }
}

void sendLockerStatusUpdates(unsigned long currentTime)
{
  if (!hardwareManager || !serverManager || !hardwareManager->getConfigurationStatus())
    return;

  LockerConfig *lockers = hardwareManager->getLockers();
  int numLockers = hardwareManager->getNumLockers();

  for (int i = 0; i < numLockers; i++)
  {
    if (currentTime - lockers[i].lastStatusUpdate > STATUS_CHECK_INTERVAL)
    {
      serverManager->sendLockerStatus(lockers[i].lockerId, lockers[i].isOccupied);
      lockers[i].lastStatusUpdate = currentTime;
    }
  }
}

void handleLockerStatusCheck()
{
  if (!hardwareManager)
    return;

  unsigned long currentTime = millis();

  if (currentTime - lastStatusCheck >= STATUS_CHECK_INTERVAL)
  {
    if (hardwareManager->checkLockerStatuses())
    {
      // Status changes detected - send updates to server
      if (serverManager && hardwareManager->getConfigurationStatus())
      {
        sendLockerStatusUpdates(currentTime);
      }
      hardwareManager->updateSystemStatus();
    }
    lastStatusCheck = currentTime;
  }
}

void handleWiFiDisconnection()
{
  if (hardwareManager)
  {
    hardwareManager->updateLCD("WiFi Lost", "Reconnecting...");
  }

  Serial.println("WiFi disconnected, attempting to reconnect...");

  if (wifiManager && wifiManager->connectToWiFi())
  {
    Serial.println("WiFi reconnected successfully");
    if (hardwareManager)
    {
      hardwareManager->updateLCD("WiFi Connected", "System Ready");
    }
  }
  else
  {
    delay(5000); // Wait before next attempt
  }
}

void performFactoryReset()
{
  Serial.println("Factory reset requested");

  if (hardwareManager)
  {
    hardwareManager->updateLCD("Factory Reset", "Please wait...");
  }

  if (wifiManager)
  {
    wifiManager->factoryReset();
  }

  // This will restart the device
}

// Cleanup function (called on restart/reset)
void cleanup()
{
  if (serverManager)
  {
    delete serverManager;
    serverManager = nullptr;
  }

  if (hardwareManager)
  {
    delete hardwareManager;
    hardwareManager = nullptr;
  }

  if (wifiManager)
  {
    delete wifiManager;
    wifiManager = nullptr;
  }

  preferences.end();
}
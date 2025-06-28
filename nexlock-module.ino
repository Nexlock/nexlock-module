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
  Serial.println(F("================================="));
  Serial.print(F("NexLock v"));
  Serial.print(FIRMWARE_VERSION);
  Serial.println(F(" starting..."));
  Serial.println(F("================================="));

  // Initialize preferences
  preferences.begin("nexlock", false);

  // Initialize managers in order
  initializeManagers();

  Serial.println(F("System initialization complete"));
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
    Serial.println(F("ERROR: Failed to create HardwareManager"));
    return;
  }

  bool hardwareReady = hardwareManager->initialize();
  Serial.print(F("Hardware: "));
  Serial.println(hardwareReady ? F("SUCCESS") : F("PENDING"));

  // Initialize WiFi manager
  wifiManager = new WiFiManager(&preferences);
  if (!wifiManager)
  {
    Serial.println(F("ERROR: Failed to create WiFiManager"));
    return;
  }

  bool wifiReady = wifiManager->initialize();
  Serial.print(F("WiFi: "));
  Serial.println(wifiReady ? F("SUCCESS") : F("PROVISIONING"));

  // Initialize server manager if WiFi is ready
  if (wifiReady)
  {
    serverManager = new ServerManager(hardwareManager, wifiManager->getMacAddress());
    if (serverManager)
    {
      bool serverReady = serverManager->initialize(wifiManager->getServerIP(), wifiManager->getServerPort());
      Serial.print(F("Server: "));
      Serial.println(serverReady ? F("SUCCESS") : F("FAILED"));
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
}

void handleManualOperations()
{
  if (!hardwareManager)
    return;

  // NFC scanning for informational purposes only
  String nfcCode;
  if (hardwareManager->scanNFC(nfcCode))
  {
    if (hardwareManager->getConfigurationStatus())
    {
      hardwareManager->updateLCD(F("NFC Detected"), F("Check app"));
      delay(1500);
      hardwareManager->updateSystemStatus();
    }
    else
    {
      hardwareManager->updateLCD(F("Not Configured"), F("Contact admin"));
      delay(1500);
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
      lockers[i].lastStatusUpdate = currentTime;
    }
  }
}

void handleWiFiDisconnection()
{
  if (hardwareManager)
  {
    hardwareManager->updateLCD(F("WiFi Lost"), F("Reconnecting..."));
  }

  Serial.println(F("WiFi disconnected, reconnecting..."));

  if (wifiManager && wifiManager->connectToWiFi())
  {
    Serial.println(F("WiFi reconnected"));
    if (hardwareManager)
    {
      hardwareManager->updateLCD(F("WiFi Connected"), F("System Ready"));
    }
  }
  else
  {
    delay(3000); // Reduced delay
  }
}

void performFactoryReset()
{
  Serial.println(F("Factory reset requested"));

  if (hardwareManager)
  {
    hardwareManager->updateLCD(F("Factory Reset"), F("Please wait..."));
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
# 🔐 NexLock - Smart Locker Management System

<div align="center">
  <img src="https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=Arduino&logoColor=white" />
  <img src="https://img.shields.io/badge/ESP32-000000?style=for-the-badge&logo=Espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/WiFi-Enabled-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/NFC-Ready-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Version-1.0.0-brightgreen?style=for-the-badge" />
</div>

## 🚀 What is NexLock?

NexLock is a cutting-edge **IoT-powered smart locker system** that brings the future of secure storage to your fingertips! Whether you're managing lockers in a gym, office, school, or any facility, NexLock provides seamless, contactless access control through NFC technology.

### ✨ Key Features

- 🎯 **NFC Access Control** - Tap to unlock with secure validation
- 📡 **WiFi Connectivity** - Real-time cloud synchronization
- 🔄 **Dynamic Configuration** - Add/remove lockers on the fly
- 📱 **Web-based Setup** - Easy WiFi provisioning via mobile
- 🖥️ **LCD Display** - Real-time status updates
- 🔧 **Modular Design** - Support for multiple locker units
- 🛡️ **Secure Communication** - WebSocket-based server integration
- 🔄 **Auto-Recovery** - Smart reconnection and error handling

## 🛠️ Hardware Requirements

### Core Components

- **ESP32 Development Board** (WiFi enabled)
- **MFRC522 NFC Reader Module**
- **16x2 I2C LCD Display**
- **3x Servo Motors** (SG90 or compatible)
- **3x IR Sensors** (for occupancy detection)
- **Push Button** (for factory reset)

### Pin Configuration

```
NFC Reader (MFRC522):
├── SS   → Pin 10
├── RST  → Pin 9
├── SDA  → Pin 10
├── SCK  → Pin 13
├── MOSI → Pin 11
└── MISO → Pin 12

Servos:
├── Servo 1 → Pin 4
├── Servo 2 → Pin 5
└── Servo 3 → Pin 6

IR Sensors:
├── IR 1 → Pin A0
├── IR 2 → Pin A1
└── IR 3 → Pin A2

Other:
├── LCD (I2C) → SDA/SCL (Pins 21/22)
└── Config Button → Pin 2
```

## 🚀 Quick Start Guide

### 1. 📥 Installation

```bash
# Clone the repository
git clone https://github.com/your-username/nexlock-ino.git
cd nexlock-ino

# Open in Arduino IDE
arduino nexlock_main.ino
```

### 2. 📚 Library Dependencies

Install these libraries through Arduino Library Manager:

```
- MFRC522 by GithubCommunity
- ArduinoJson by Benoit Blanchon
- SocketIOclient by Markus Sattler
- LiquidCrystal_I2C by Frank de Brabander
- ESP32Servo by Kevin Harrington
```

### 3. ⚡ First Boot Setup

1. **Flash the firmware** to your ESP32
2. **Power on** the device
3. **Connect to WiFi AP**: `NexLock_[MAC_ADDRESS]` (Password: `12345678`)
4. **Open browser** and go to `192.168.4.1`
5. **Enter your WiFi credentials** and server details
6. **Save configuration** - device will restart automatically!

### 4. 🎛️ Admin Configuration

Once connected to WiFi, the device will broadcast its availability. Use your admin panel to:

- Assign locker IDs
- Configure access permissions
- Monitor real-time status

## 📁 Project Structure

```
nexlock-ino/
├── 📄 nexlock_main.ino          # Main application entry point
├── 📄 config.h                  # Hardware & timing configurations
├── 📄 wifi_manager.h/.cpp        # WiFi provisioning & management
├── 📄 hardware_manager.h/.cpp    # NFC, servos, LCD, IR sensors
├── 📄 server_manager.h/.cpp      # WebSocket communication
├── 📄 microprojfinal.ino         # Legacy file (deprecated)
└── 📄 README.md                  # You are here! 👋
```

## 🔧 Configuration

### WiFi Provisioning

The device creates a captive portal for easy setup:

- **SSID**: `NexLock_[MAC_ADDRESS]`
- **Password**: `12345678`
- **Portal**: `http://192.168.4.1`

### Server Communication

Configure your backend server details:

- **Server IP**: Your NexLock server address
- **Port**: Default 3000 (Socket.IO)
- **Protocol**: WebSocket with Socket.IO

### Hardware Thresholds

Customize IR sensor sensitivity in `config.h`:

```cpp
#define IR_THRESHOLD_NORMAL 950   // Lockers 1 & 2
#define IR_THRESHOLD_LOCKER3 820  // Locker 3 (if different)
```

## 🎮 Usage

### Normal Operation

1. **Scan NFC card** on the reader
2. **Wait for validation** (LCD shows "Validating...")
3. **Access granted** - servo unlocks assigned locker
4. **Close locker** - scan again to lock

### Factory Reset

Hold the **config button for 5 seconds** to perform factory reset:

- Clears all stored configurations
- Returns to provisioning mode
- Requires complete reconfiguration

## 🔌 API Integration

### WebSocket Events

#### Outgoing Events

```javascript
// Module registration
"register" → { moduleId: "string" }

// NFC validation request
"validate-nfc" → { nfcCode: "string", moduleId: "string" }

// Locker status update
"locker-status" → {
  moduleId: "string",
  lockerId: "string",
  occupied: boolean,
  timestamp: number
}

// Heartbeat
"ping" → { moduleId: "string" }

// Module availability broadcast
"module-available" → {
  macAddress: "string",
  deviceInfo: "string",
  version: "string",
  capabilities: number
}
```

#### Incoming Events

```javascript
// Module configuration
"module-configured" → {
  moduleId: "string",
  lockerIds: ["string", "string", ...]
}

// Remote unlock command
"unlock" → {
  lockerId: "string",
  action: "unlock"
}

// NFC validation response
"nfc-validation-result" → {
  valid: boolean,
  lockerId: "string",
  message: "string"
}
```

## 🐛 Troubleshooting

### Common Issues

**🔴 WiFi Connection Failed**

- Check credentials in provisioning portal
- Ensure 2.4GHz WiFi network
- Verify signal strength

**🔴 NFC Not Reading**

- Check MFRC522 wiring
- Verify 3.3V power supply
- Test with known NFC cards

**🔴 Servo Not Moving**

- Confirm servo power (5V recommended)
- Check pin connections
- Test servo positions in code

**🔴 Server Connection Issues**

- Verify server IP and port
- Check firewall settings
- Monitor serial output for debugging

### Debug Mode

Enable verbose logging by adding to `setup()`:

```cpp
Serial.setDebugOutput(true);
```

## 🤝 Contributing

We love contributions! Here's how you can help:

1. 🍴 **Fork** the repository
2. 🌿 **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. 💾 **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. 📤 **Push** to the branch (`git push origin feature/amazing-feature`)
5. 🎯 **Open** a Pull Request

### Development Guidelines

- Follow Arduino coding standards
- Comment your code thoroughly
- Test on actual hardware before submitting
- Update documentation for new features

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

## 🏆 Credits

Created with ❤️ by the NexLock team

### Special Thanks

- **MFRC522 Library** - RC522 NFC support
- **ArduinoJson** - JSON parsing and generation
- **SocketIOclient** - Real-time communication
- **ESP32 Community** - Amazing hardware platform

## 📞 Support

Need help? We've got you covered!

- 📧 **Email**: support@nexlock.com
- 💬 **Discord**: [Join our community](https://discord.gg/nexlock)
- 🐛 **Issues**: [GitHub Issues](https://github.com/your-username/nexlock-ino/issues)
- 📖 **Documentation**: [Full Docs](https://docs.nexlock.com)

---

<div align="center">
  <h3>🌟 Star this repository if you found it helpful! 🌟</h3>
  <p>Made with 💖 and lots of ☕</p>
</div>

# BRMesh Home Assistant
Here in Canada, it seems like it's impossible to get outdoor floodlights that are compatible with Home Assistant.  All the available options on Amazon employ the "BRMesh" app. The app admittedly works well, but of course I want Home Assistant integration, which this completely offline app does not support. The lights themselves are sold under several brand names, such as MELPO, SOLLA, Neliwo, but they seem to all be the same light. 

This is my solution. This is firmware for an ESP32 that will allow control of "BRMesh" lights.

## Requirements
### Software Requirements
- Home Assistant MQTT Broker Add-on installed in Home Assistant (https://www.youtube.com/watch?v=dqTn-Gk4Qeo&ab_channel=EverythingSmartHome)
- Home Assistant MQTT Integration added in Home Assistant (https://www.home-assistant.io/integrations/mqtt/)
- Arduino IDE (install library "home-assistant-integration" (Dawid Chyrzynski) in the Library Manager)
- BRMesh app installed on Android phone
- Android `adb` installed on computer (https://www.xda-developers.com/install-adb-windows-macos-linux)

### Hardware Requirements
- Android phone (only for accessing BRMesh encryption key)
- ESP32 that will act as the controller
- A light (obviously)

## Instructions

Several things must be modified in the sketch:

```cpp
//IP Address of your MQTT Broker (probably your Home Assistant host)
#define MQTT_BROKER_ADDR IPAddress(192,168,0,100)
//Your MQTT UserName
#define MQTT_BROKER_USER "User123"
//Your MQTT Password
#define MQTT_BROKER_PASS "password1234"
//Your WiFi SSID
#define WIFI_SSID "MyWifi"
//Your Wifi Password
#define WIFI_PASS "wifipassword"

const uint8_t my_key[] = { 0x38, 0x35, 0x31, 0x33 }; //Unique key from BRMesh app (found using USB debugging and adb logcat)
byte mac[] = {0x14, 0x39, 0x28, 0x32, 0xa7, 0xb0}; // find in serial console upon reset, e.g. "start ESP32 DEVICEID - AABBCCDDEEFF"  (<-that's the MAC)

const int redundancy = 5;  // Repeats sending each command to the lights this many times; BLE broadcasting was flakey
```


## Acknowledgements
- The bluetooth communication was hacked together by modifying Jueff's ESP code from this thread: https://community.home-assistant.io/t/brmesh-app-bluetooth-lights/473486/22
- The MQTT integration is provided by Dawid Chyrzynski's awesome MQTT/HomeAssistant/Arduino library here: https://github.com/dawidchyrzynski/arduino-home-assistant

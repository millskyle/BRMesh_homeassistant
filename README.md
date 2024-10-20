# BRMesh Home Assistant
Here in Canada, it seems like it's impossible to get outdoor floodlights that are compatible with Home Assistant.  All the available options on Amazon employ the "BRMesh" app. The app admittedly works well, but of course I want Home Assistant integration, which this completely offline app does not support. The lights themselves are sold under several brand names, such as MELPO, SOLLA, Neliwo, but they seem to all be the same light. 

This solves my problem and integrates the lights into HomeAssistant. This is firmware for an ESP32 that will allow control of "BRMesh" lights.

## Requirements
### Software Requirements
- Home Assistant MQTT Broker Add-on installed in Home Assistant (https://www.youtube.com/watch?v=dqTn-Gk4Qeo&ab_channel=EverythingSmartHome)
- Home Assistant MQTT Integration added in Home Assistant (https://www.home-assistant.io/integrations/mqtt/)
- Arduino IDE (install library "home-assistant-integration" (Dawid Chyrzynski) in the Library Manager)
- ESP32 Board Core v2.0.14 ** (see "Known Issues" below)
- BRMesh app installed on Android phone
- Android `adb` installed on computer (https://www.xda-developers.com/install-adb-windows-macos-linux)

### Hardware Requirements
- Android phone (only for accessing BRMesh encryption key)
- ESP32 that will act as the controller
- A light (obviously)

## Instructions

After making the modifications listed below, you should be able to flash an ESP with this firmware, and it will automatically talk to Home Assistant and announce the lights. They will appear in the MQTT integration as a "BRMesh" light.  It seemed like magic to me when it happened. I was shocked it worked.


### Modifications: 

Several things must be modified in the sketch. You'll want to take a look at this section and modify it before flashing:

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

### Obtaining `my_key` from BRMesh
1. Add the lights you wish to control to BRMesh app
2. Plug your Android phone into your computer and run the command:
   ```bash
   adb logcat  | grep getPayloadWithInner
   ```
3. Accept the warning on your phone
4. Change the color of your lights, turn them on and off, etc. You should see some things printed to your terminal like this:
```   
11-27 21:25:23.309  7827  7827 I jyq_helper: getPayloadWithInnerRetry---> payload:22035b000000000000000000,  key: 31323334
11-27 21:25:23.309  7827  7827 I jyq_helper: getPayloadWithInnerRetry---> mSendCnt:95,  sSendSeq:243,  seq:243
11-27 21:25:28.338  7827  7827 I jyq_helper: getPayloadWithInnerRetry---> payload:22037f000000000000000000,  key: 31323334
11-27 21:25:28.338  7827  7827 I jyq_helper: getPayloadWithInnerRetry---> mSendCnt:96,  sSendSeq:244,  seq:244
11-27 21:25:29.567  7827  7827 I jyq_helper: getPayloadWithInnerRetry---> payload:7203ff00ff00000000000000,  key: 31323334
11-27 21:25:29.567  7827  7827 I jyq_helper: getPayloadWithInnerRetry---> mSendCnt:97,  sSendSeq:245,  seq:245
```
6. Write down the "key". In this case, 31323334 means you'll input `const uint8_t my_key[] = { 0x31, 0x32, 0x33, 0x34 };` in the Arduino sketch.

### MQTT and Wifi Credentials
Set you MQTT and Wifi credentials.  Nothing to really explain here. If you need to change your MQTT port, I've hardcoded that and you can find it in the code in the `mqtt.begin()` call.

### Set the MAC address
If you don't know the MAC address of your ESP32, you can flash the firmware with the incorrect MAC address, and watch in the serial monitor. Upon boot, the ESP prints its true MAC address. Fill this in the sketch and re-flash.

### Modify the number of lights
I bought a 4-pack of these lights, and thus wrote the code for 4 lights. It can be easily modified in the sketch by changing the `numLights`, `mylights`, and `mylightnames` declarations.

### Flashing
I had an issue with flashing my ESP32 (I'm pretty new to Arduino IDE...maybe this is obvious), but I had to go to Tools > Partition Scheme and change it to "Huge APP (3MB...)" or I got a "sketch too large" error.




## Known issues

### String error (ESP board core v2.0.14)
This breaks on newer versions of ESP board core. v2.0.14 is what I used during development. In Arduino IDE, go to Tools > Board > Board Manager; search "esp32"; downgrade to 2.0.14. If you get a new error about FQBN, try restarting the IDE, and if that doesn't work, try clearing your Arduino appdata.

### White temperature
My light did not have color temperature, so I've omitted it as I could not decode the BRMesh payload for such commands without owning a compatible light. It should be possible to modify this if the payloads can be obtained.

### Incorrect States
The ESP tries to keep track of and report the correct state of the lights to HomeAssistant, but since the lights don't communicate (only receive broadcast commands), there's no way to tell if a light received the command sent to it, or if it has been changed via another method (e.g. the BRMesh app).  Thus the state reported in Home Assistant should not be taken as absolute truth about the state of the light.

### Commands not received by light 
Only about 80% of the commands that I sent to the lights actually were received. Thus I introduced the `redundancy` variable in the firmware. Each command is sent `redundancy` times (defaultt: 5) very quickly in series. This seems to solve almost all issues. If you're having problems with commands being flakey, try upping this.




## Acknowledgements
- The bluetooth communication was hacked together by modifying Jueff's ESP code from this thread: https://community.home-assistant.io/t/brmesh-app-bluetooth-lights/473486/22
- The MQTT integration is provided by Dawid Chyrzynski's awesome MQTT/HomeAssistant/Arduino library here: https://github.com/dawidchyrzynski/arduino-home-assistant

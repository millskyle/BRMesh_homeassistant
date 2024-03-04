#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEServer.h"
#include "BLEBeacon.h"
#include <WiFi.h>
#include <ArduinoHA.h>

WiFiClient client;
HADevice device("BRMesh");
HAMqtt mqtt(client, device);

// check logs of android device:
// adb logcat | grep getPayloadWithInner

//////////////////////////////////////////////////////
//CONFIGURATION
//////////////////////////////////////////////////////
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
const int redundancy = 5;  // Repeats sending each command to the lights this many times; BLE broadcasting was flakey  

// LIGHT DEFINITION
//    Add/remove lights here:  (numLights, create an additional HALight object,
//    and add to the list of light names.
//    If you want to adda Group the name must contan "Group"
//    Add the corresponding id in mylightids

const int numLights = 5;
HALight mylights[numLights] = {
  //add one line for each light.
  HALight("1", HALight::BrightnessFeature | HALight::ColorTemperatureFeature),
  HALight("2", HALight::BrightnessFeature | HALight::ColorTemperatureFeature),
  HALight("3", HALight::BrightnessFeature | HALight::ColorTemperatureFeature),
  HALight("4", HALight::BrightnessFeature | HALight::RGBFeature),
  HALight("5", HALight::BrightnessFeature | HALight::ColorTemperatureFeature)
};

String mylightnames[numLights] = {"WC", "Conference1", "Conference2", "Conference3", "ConferenceGroup"};
uint8_t mylightids[numLights] = {1,2,3,4,0xc3};



////////////////////////////////////////////////
// You probably don't need to modify below here
////////////////////////////////////////////////

BLEAdvertising *pAdvertising;   // BLE Advertisement type
#define BEACON_UUID "87b99b2c-90fd-11e9-bc42-526af7764f64" // UUID 1 128-Bit (may use linux tool uuidgen or random numbers via https://www.uuidgenerator.net/)

const uint8_t default_key[] = { 0x5e, 0x36, 0x7b, 0xc4 };

void setup() {
  //BLE STUFF
  Serial.begin(115200);
  Serial.printf("start ESP32 DEVICEID - %llX\n", ESP.getEfuseMac());

  // Create the BLE Device
  BLEDevice::init("ESP32 as iBeacon");

  pAdvertising = BLEDevice::getAdvertising();
  BLEDevice::startAdvertising();

  //HOME ASSISTANT MQTT STUFF
  // you don't need to verify return status
  device.setName("BRMesh");
  device.setManufacturer("BRMesh");
  device.setModel("BRMesh");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500); // waiting for the connection
      Serial.print("."); // Print dots to indicate waiting
  }
  Serial.println("\nConnected to WiFi.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  for (int i=0; i<numLights; i++) 
  {
    mylights[i].setName(mylightnames[i].c_str());
    mylights[i].onStateCommand(onStateCommand);
    mylights[i].onBrightnessCommand(onBrightnessCommand); // optional
    mylights[i].onColorTemperatureCommand(onColorTemperatureCommand); // optional
    mylights[i].onRGBColorCommand(onRGBColorCommand); // optional
    mylights[i].setBrightnessScale(127);
  }
  
  Serial.println("Starting MQTT");
  mqtt.begin(MQTT_BROKER_ADDR, 1883, MQTT_BROKER_USER, MQTT_BROKER_PASS);
}

void loop() 
{
    mqtt.loop();
}

uint8_t get_id(String x){
  for(uint8_t r = 0; r <= numLights;r++)
  {
    if(x.equals(mylightnames[r]))
      return mylightids[r];
  }
  return 0;
}

void send(uint8_t* data, uint8_t dataLength);

uint8_t SEND_SEQ = 0;
uint8_t SEND_COUNT = 1;

const uint8_t BLE_CMD_RETRY_CNT = 1;
const uint8_t DEFAULT_BLE_FASTCON_ADDRESS[] = { 0xC1, 0xC2, 0xC3 };
const uint8_t addrLength = 3;

void dump(const uint8_t* data, int length)
{
  for (int i = 0; i < length; i++)
  {
    printf("%2.2X", data[i]);
  }
}

uint8_t package_ble_fastcon_body(int i, int i2, uint8_t sequence, uint8_t safe_key, int forward, const uint8_t* data, int length, const uint8_t* key, uint8_t*& payload)
{
  if (length > 12)
  {
    printf("data too long");
    payload = 0;
    return 0;
  }
  uint8_t payloadLength = 4 + 12;
  payload = (uint8_t*)malloc(payloadLength);
  payload[0] = (i2 & 0b1111) << 0 | (i & 0b111) << 4 | (forward & 0xff) << 7;
  payload[1] = sequence & 0xff;
  payload[2] = safe_key;
  payload[3] = 0; // checksum
  // fill payload with zeros
  for (int j = 4; j < payloadLength; j++) payload[j]=0;
  memcpy(payload + 4, data, length);

  uint8_t checksum = 0;
  for (int j = 0; j < length + 4; j++) 
  {
    if (j == 3) continue;
    checksum = (checksum + payload[j]) & 0xff;
  }
  payload[3] = checksum;
  for (int j = 0; j < 4; j++) {
    payload[j] = default_key[j & 3] ^ payload[j];
  }
  for (int j = 0; j < 12; j++) {
    payload[4 + j] = my_key[j & 3] ^ payload[4 + j];
  }
  return payloadLength;
}

uint8_t get_payload_with_inner_retry(int i, const uint8_t* data, int length, int i2, const uint8_t* key, int forward, uint8_t*& payload) {
  printf("data: "); dump(data, length); printf("\n");

  SEND_COUNT++;
  SEND_SEQ = SEND_COUNT;
  uint8_t safe_key = key[3];
  return package_ble_fastcon_body(i, i2, SEND_SEQ, safe_key, forward, data, length, key, payload);
}

void whiteningInit(uint8_t val, uint8_t* ctx)
{
  ctx[0] = 1;
  ctx[1] = (val >> 5) & 1;
  ctx[2] = (val >> 4) & 1;
  ctx[3] = (val >> 3) & 1;
  ctx[4] = (val >> 2) & 1;
  ctx[5] = (val >> 1) & 1;
  ctx[6] = val & 1;
}

void whiteningEncode(const uint8_t* data, int len, uint8_t* ctx, uint8_t* result)
{
  memcpy(result, data, len);
  for (int i = 0; i < len; i++) {
    int varC = ctx[3];
    int var14 = ctx[5];
    int var18 = ctx[6];
    int var10 = ctx[4];
    int var8 = var14 ^ ctx[2];
    int var4 = var10 ^ ctx[1];
    int _var = var18 ^ varC;
    int var0 = _var ^ ctx[0];

    int c = result[i];
    result[i] = ((c & 0x80) ^ ((var8 ^ var18) << 7))
      + ((c & 0x40) ^ (var0 << 6))
      + ((c & 0x20) ^ (var4 << 5))
      + ((c & 0x10) ^ (var8 << 4))
      + ((c & 0x08) ^ (_var << 3))
      + ((c & 0x04) ^ (var10 << 2))
      + ((c & 0x02) ^ (var14 << 1))
      + ((c & 0x01) ^ (var18 << 0));

    ctx[2] = var4;
    ctx[3] = var8;
    ctx[4] = var8 ^ varC;
    ctx[5] = var0 ^ var10;
    ctx[6] = var4 ^ var14;
    ctx[0] = var8 ^ var18;
    ctx[1] = var0;
  }
}

uint8_t reverse_8(uint8_t d)
{
  uint8_t result = 0;
  for (uint8_t k = 0; k < 8; k++) {
    result |= ((d >> k) & 1) << (7 - k);
  }
  return result;
}

uint16_t reverse_16(uint16_t d) {
  uint16_t result = 0;
  for (uint8_t k = 0; k < 16; k++) {
    result |= ((d >> k) & 1) << (15 - k);
  }
  return result;
}

uint16_t crc16(const uint8_t* addr, const uint8_t* data, uint8_t dataLength)
{
  uint16_t crc = 0xffff;

  for (int8_t i = addrLength - 1; i >= 0; i--) 
  {
    crc ^= addr[i] << 8;
    for (uint8_t ii = 0; ii < 4; ii++) {
      uint16_t tmp = crc << 1;

      if ((crc & 0x8000) !=0)
      {
        tmp ^= 0x1021;
      }

      crc = tmp << 1;
      if ((tmp & 0x8000) != 0)
      {
        crc ^= 0x1021;
      }
    }
  }

  for (uint8_t i = 0; i < dataLength; i++) {
    crc ^= reverse_8(data[i]) << 8;
    for (uint8_t ii = 0; ii < 4; ii++) {
      uint16_t tmp = crc << 1;

      if ((crc & 0x8000) != 0) 
      {
        tmp ^= 0x1021;
      }

      crc = tmp << 1;
      if ((tmp & 0x8000) != 0)
      {
        crc ^= 0x1021;
      }
    }
  }
  crc = ~reverse_16(crc) & 0xffff;
  return crc;
}


uint8_t get_rf_payload(const uint8_t* addr, const uint8_t* data, uint8_t dataLength, uint8_t*& rfPayload)
{

  uint8_t data_offset = 0x12;
  uint8_t inverse_offset = 0x0f;
  uint8_t result_data_size = data_offset + addrLength + dataLength+2;
  uint8_t* resultbuf = (uint8_t*)malloc(result_data_size);
  memset(resultbuf, 0, result_data_size);

  resultbuf[0x0f] = 0x71;
  resultbuf[0x10] = 0x0f;
  resultbuf[0x11] = 0x55;


  for (uint8_t j = 0; j < addrLength; j++) {
    resultbuf[data_offset + addrLength - j - 1] = addr[j];
  }

  for (int j = 0; j < dataLength; j++) {
    resultbuf[data_offset + addrLength + j] = data[j];
  }


  for (int i = inverse_offset; i < inverse_offset + addrLength + 3; i++) {
    resultbuf[i] = reverse_8(resultbuf[i]);
  }

  int crc = crc16(addr, data, dataLength);
  resultbuf[result_data_size-2] = crc & 0xff;
  resultbuf[result_data_size-1] = (crc >> 8) & 0xff;
  rfPayload = resultbuf;
  return result_data_size;
}

uint8_t do_generate_command(int i, const uint8_t* data, uint8_t length, const uint8_t* key, int forward, int use_default_adapter, int i2, uint8_t*& rfPayload)
{
  if (i2 < 0) i2 = 0;
  uint8_t* payload = 0;
  uint8_t* rfPayloadTmp = 0;
  uint8_t payloadLength = get_payload_with_inner_retry(i, data, length, i2, key, forward, payload);
  uint8_t rfPayloadLength = get_rf_payload(DEFAULT_BLE_FASTCON_ADDRESS, payload, payloadLength, rfPayloadTmp);
  free(payload);

  uint8_t ctx[7];
  whiteningInit(0x25, &ctx[0]);
  uint8_t* result = (uint8_t*)malloc(rfPayloadLength);

  whiteningEncode(rfPayloadTmp, rfPayloadLength, ctx, result);

  rfPayload = (uint8_t*)malloc(rfPayloadLength-15);
  memcpy(rfPayload, result + 15, rfPayloadLength - 15);
  free(result);
  free(rfPayloadTmp);
  return rfPayloadLength-15;
}

void single_control(const uint8_t* key, const uint8_t* result)
{
  uint8_t ble_adv_data[] = { 0x02, 0x01, 0x1A, 0x1B, 0xFF, 0xF0, 0xFF };
  uint8_t* rfPayload = 0;
  uint8_t rfPayloadLength = do_generate_command(5, result, 12, key, true /* forward ?*/, true /* use_default_adapter*/, 0, rfPayload);
  uint8_t* advPacket = (uint8_t*)malloc(rfPayloadLength + sizeof(ble_adv_data));
  memcpy(advPacket, ble_adv_data, sizeof(ble_adv_data));
  memcpy(advPacket+ sizeof(ble_adv_data), rfPayload, rfPayloadLength);
  free(rfPayload);

  for (int i=0; i<redundancy; i++) {
    send(advPacket, rfPayloadLength + sizeof(ble_adv_data));
    delay(50);
  }
  free(advPacket);
}

void send(uint8_t* data, uint8_t dataLength)
{
  BLEBeacon oBeacon = BLEBeacon();
  oBeacon.setManufacturerId(0xf0ff); // fake Apple 0x004C LSB (ENDIAN_CHANGE_U16!)
  oBeacon.setProximityUUID(BLEUUID(BEACON_UUID));
  oBeacon.setMajor(0);
  oBeacon.setMinor(0);
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();
  oAdvertisementData.setFlags(0x04); // BR_EDR_NOT_SUPPORTED 0x04
  std::string strServiceData = "";
  strServiceData += (char)(dataLength-4);     // Len  
  for (int i=4;i<dataLength;i++)
  {
    strServiceData += (char)data[i];
  }
  oAdvertisementData.addData(strServiceData);

  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setScanResponseData(oScanResponseData);
  pAdvertising->start();
  delay(50);
  pAdvertising->stop();
}


//    uint8_t data[] = { 0x72, 0x00, 0xff, 0xff, 0x1d, 0xff };
//    single_control(my_key, data);

void onStateCommand(bool state, HALight* sender) {
  uint8_t brightness = sender->getCurrentBrightness();
  if (state) {
    if (brightness == 0)
      brightness = 0x7F;
    onBrightnessCommand(brightness,sender);
  } else {
    onBrightnessCommand(0,sender);
  }
}

void onBrightnessCommand(uint8_t brightness, HALight* sender) {
  uint8_t g = 0;
  Serial.print("Light:");
  Serial.println(sender->getName());
  if (brightness > 127) brightness = 127;
  Serial.print("Brightness: ");
  Serial.println(brightness);
  
  uint8_t data[] = { 0x22, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  String name = sender->getName();
  if(name.indexOf("Group") > 0)
  {
    g = 2;
    data[0] += 0x21;
    data[1] =  0x2a;
    data[2] =  0xa8;
  }
  data[1+g] = get_id(sender->getName());
  data[2+g] = brightness & 127;

  single_control(my_key, data);
  if(brightness > 0) //report brightness back to the Home Assistant only if turned on
  {
    sender->setBrightness(brightness); 
    sender->setState(1); // report state back to the Home Assistant
  }
  else
  {
    sender->setState(0); // report state back to the Home Assistant
  }
}

void onColorTemperatureCommand(uint16_t temperature, HALight* sender) {
  uint8_t a,b;
  uint8_t g = 0;
  Serial.print("Color temperature: ");
  Serial.println(temperature);
  uint8_t brightness = sender->getCurrentBrightness();
  if (brightness < 2)
    brightness = 127;

  if (temperature < 153) {
    a = 0xff;
    b = 0x00;
  } else if (temperature > 500) {
    a = 0x00;
    b = 0xff;
  } else {
    // Linear interpolation between (153, 0xff) and (500, 0x00)
    a = (uint8_t)(((500 - temperature) * 0xff + (temperature - 153) * 0x00) / (500 - 153));
    b = (uint8_t)(((temperature - 153) * 0xff + (500 - temperature) * 0x00) / (500 - 153));
  }

  uint8_t data[] = { 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00 };
  String name = sender->getName();
  if(name.indexOf("Group") > 0)
  {
    g = 2;
    data[0] += 0x21;
    data[1] =  0x2a;
    data[2] =  0xa8;
  }
  data[1+g] = get_id(sender->getName());
  data[2+g] = brightness & 127;
  data[6+g] = b;
  data[7+g] = a;
  
  single_control(my_key, data);

  sender->setColorTemperature(temperature); // report color temperature back to the Home Assistant
  sender->setBrightness(brightness); 
}

void onRGBColorCommand(HALight::RGBColor color, HALight* sender) {
  uint8_t g = 0;
  Serial.print("Light:");
  Serial.println(sender->getName());
  Serial.print("Red: ");
  Serial.println(color.red);
  Serial.print("Green: ");
  Serial.println(color.green);
  Serial.print("Blue: ");
  Serial.println(color.blue);
  uint8_t data[] = { 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  String name = sender->getName();
  if(name.indexOf("Group") > 0)
  {
    g = 2;
    data[0] += 0x21;
    data[1] =  0x2a;
    data[2] =  0xa8;
  }
  data[1+g] = get_id(sender->getName());
  data[2+g] = sender->getCurrentBrightness() & 127;
  data[3+g] = color.blue;
  data[4+g] = color.red;
  data[5+g] = color.green;
  
  single_control(my_key, data);
  sender->setRGBColor(color); // report color back to the Home Assistant
}

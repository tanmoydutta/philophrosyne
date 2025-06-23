#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CSen5x.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define INDICATOR_LED 13
// UUIDs
#define ENV_SERVICE_UUID "d0000000-0000-1000-8000-00805f9b34fb"
#define TEMP_UUID        "2A6E"
#define HUM_UUID         "2A6F"
#define VOC_UUID         "d0000004-0000-1000-8000-00805f9b34fb"
#define NOX_UUID         "d0000005-0000-1000-8000-00805f9b34fb"
#define PM25_UUID        "2BD3"

// The used commands use up to 48 bytes. On some Arduino's the default buffer
// space is not large enough
#define MAXBUF_REQUIREMENT 48

#if (defined(I2C_BUFFER_LENGTH) &&                 \
     (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || \
    (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT)
#define USE_PRODUCT_INFO
#endif
// 
SensirionI2CSen5x sen5x;

unsigned long previousMillis = 0;  // will store last time LED was updated

// constants won't change:
const long interval = 30000;  // interval at which to blink (milliseconds)

// Helper: encode float to IEEE-11073 SFLOAT (2 bytes)
uint16_t encodeSFloat(float value) {
  if (isnan(value)) return 0x07FF;
  int8_t exponent = 0;
  int16_t mantissa = (int16_t)value;
  while ((mantissa < -2048 || mantissa > 2047) && exponent < 7) {
    exponent++;
    mantissa /= 10;
  }
  if (mantissa < -2048 || mantissa > 2047) return 0x07FF;
  return ((exponent & 0x0F) << 12) | (mantissa & 0x0FFF);
}

// Characteristic callback class for pull reads
class SensorCallback : public BLECharacteristicCallbacks {
  int sensorType;
public:
  SensorCallback(int type) : sensorType(type) {}

  void onRead(BLECharacteristic* pCharacteristic) override {
    float temp, hum, voc, nox, pm1p0, pm2p5, pm4p0, pm10p0;

    int16_t err = sen5x.readMeasuredValues(pm1p0, pm2p5, pm4p0, pm10p0, hum, temp, voc, nox);
    if (err != 0) {
      Serial.print("Sensor read error: ");
      Serial.println(err);
      return;
    }

    float val = 0.0;
    switch(sensorType) {
      case 0: val = temp; break;
      case 1: val = hum; break;
      case 2: val = voc; break;
      case 3: val = nox; break;
      case 4: val = pm2p5; break;
      default: val = 0; break;
    }

    uint16_t encoded = encodeSFloat(val);
    uint8_t buf[2] = { (uint8_t)(encoded & 0xFF), (uint8_t)(encoded >> 8) };
    pCharacteristic->setValue(buf, 2);

    Serial.print("Sent value type ");
    Serial.print(sensorType);
    Serial.print(": ");
    Serial.println(val);
  }
};

// Helper to create characteristic with callback
BLECharacteristic* createChar(BLEService* service, const char* uuid, const char* description, int type) {
  BLECharacteristic* ch = service->createCharacteristic(uuid, BLECharacteristic::PROPERTY_READ);
  ch->setCallbacks(new SensorCallback(type));

  BLEDescriptor* desc = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
  desc->setValue(description);
  ch->addDescriptor(desc);

  return ch;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Client connected.");
  }

  void onDisconnect(BLEServer* pServer) {
    Serial.println("Client disconnected. Restarting advertising...");
    delay(100);
    pServer->startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  //while (!Serial){}
  pinMode(INDICATOR_LED, OUTPUT);
  Wire.begin();

  sen5x.begin(Wire);
//////
    uint16_t error;
    char errorMessage[256];
    error = sen5x.deviceReset();
    if (error) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
    //delay(2000);


    float tempOffset = 0.0;
    error = sen5x.setTemperatureOffsetSimple(tempOffset);
    if (error) {
        Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("Temperature Offset set to ");
        Serial.print(tempOffset);
        Serial.println(" deg. Celsius (SEN54/SEN55 only");
    }
    delay(2000);
    // Start Measurement
    error = sen5x.startMeasurement();
    if (error) {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
//////



  BLEDevice::init("ESP32-SEN54");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService* service = server->createService(ENV_SERVICE_UUID);
/////

/////
  createChar(service, TEMP_UUID, "Temperature (°C)", 0);
  createChar(service, HUM_UUID, "Humidity (%)", 1);
  createChar(service, VOC_UUID, "VOC Index", 2);
  createChar(service, NOX_UUID, "NOx Index", 3);
  createChar(service, PM25_UUID, "PM2.5 (ug/m3)", 4);

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(ENV_SERVICE_UUID);
  advertising->start();

  Serial.println("BLE server started. Waiting for clients...");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    digitalWrite(INDICATOR_LED, LOW);
    delay(25);
    digitalWrite(INDICATOR_LED, HIGH);
    delay(1000);
  } else {
    delay(1000);
  }
}

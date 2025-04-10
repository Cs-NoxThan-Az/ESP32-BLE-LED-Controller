#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pLedStateCharacteristic = NULL;
BLECharacteristic* pLedBrightnessCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

const int ledPin = 2;  // LED pin (PWM capable)
const int pwmFrequency = 5000;  // PWM frequency (Hz)
const int pwmResolution = 8;  // PWM resolution (8 bits = 0-255)

bool ledState = false;  // LED state (on/off)
int brightness = 128;   // Default brightness value (0-255)
int savedBrightness = 128;  // To store brightness when LED is turned off

// Define a clear device name
#define DEVICE_NAME "ESP32_LED_CTRL"
#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define LED_STATE_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define BRIGHTNESS_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Client connected");
    BLEDevice::startAdvertising(); // Keep advertising even when connected
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected");
    // Restart advertising immediately
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted after disconnection");
  }
};

class LedStateCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (pCharacteristic->getValue().length() > 0) {
      uint8_t state = pCharacteristic->getValue()[0];
      Serial.print("LED State: ");
      Serial.println(state);
      
      ledState = (state == 1);
      
      if (ledState) {
        // Turn ON with saved brightness
        brightness = savedBrightness;
        ledcWrite(ledPin, brightness);
        Serial.print("LED ON with brightness: ");
        Serial.println(brightness);
      } else {
        // Turn OFF but save current brightness
        savedBrightness = brightness;
        ledcWrite(ledPin, 0);
        Serial.print("LED OFF, saved brightness: ");
        Serial.println(savedBrightness);
      }
      
      // Update brightness characteristic with current value
      uint8_t brightnessValue = brightness;
      pLedBrightnessCharacteristic->setValue(&brightnessValue, 1);
      pLedBrightnessCharacteristic->notify();
    }
  }
};

class BrightnessCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (pCharacteristic->getValue().length() > 0) {
      uint8_t newBrightness = pCharacteristic->getValue()[0];
      Serial.print("Brightness value received: ");
      Serial.println(newBrightness);
      
      // Update the brightness value
      brightness = newBrightness;
      savedBrightness = brightness;
      
      // Only apply brightness if LED is ON
      if (ledState) {
        ledcWrite(ledPin, brightness);
        Serial.print("Setting brightness to: ");
        Serial.println(brightness);
      } else {
        Serial.println("LED is OFF, brightness will be applied when turned on");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE LED Controller...");
  
  // Configure LED PWM
  if (ledcAttach(ledPin, pwmFrequency, pwmResolution)) {
    Serial.println("PWM setup successful");
  } else {
    Serial.println("PWM setup failed");
  }
  
  ledcWrite(ledPin, 0); // Start with LED off
  
  // Create the BLE Device
  BLEDevice::init(DEVICE_NAME);
  Serial.print("BLE Device initialized with name: ");
  Serial.println(DEVICE_NAME);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create LED state characteristic (On/Off)
  pLedStateCharacteristic = pService->createCharacteristic(
                      LED_STATE_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pLedStateCharacteristic->addDescriptor(new BLE2902());
  pLedStateCharacteristic->setCallbacks(new LedStateCallbacks());

  // Create LED brightness characteristic (0-255)
  pLedBrightnessCharacteristic = pService->createCharacteristic(
                      BRIGHTNESS_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pLedBrightnessCharacteristic->addDescriptor(new BLE2902());
  pLedBrightnessCharacteristic->setCallbacks(new BrightnessCallbacks());

  // Set initial values
  uint8_t initialState = ledState ? 1 : 0;
  pLedStateCharacteristic->setValue(&initialState, 1);
  
  uint8_t initialBrightness = brightness;
  pLedBrightnessCharacteristic->setValue(&initialBrightness, 1);

  // Start the service
  pService->start();
  Serial.println("BLE service started");

  // Configure advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  
  // Set advertising parameters for better connection
  pAdvertising->setMinInterval(32); // 20ms
  pAdvertising->setMaxInterval(64); // 40ms
  
  BLEDevice::startAdvertising();
  
  Serial.println("BLE advertising started!");
  Serial.println("BLE LED Controller is ready!");
}

void loop() {
  // Handle disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Give the Bluetooth stack time to get ready
    pServer->startAdvertising(); // Restart advertising
    Serial.println("Advertising restarted");
    oldDeviceConnected = deviceConnected;
  }
  
  // Handle new connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  // Status update
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 5000) {
    lastStatusTime = millis();
    Serial.print("BLE Status: ");
    Serial.println(deviceConnected ? "Connected" : "Ready for connection");
  }
}
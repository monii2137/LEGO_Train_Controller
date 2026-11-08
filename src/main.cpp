
#include <Arduino.h>
#include <WiFi.h>
#include "wifi_secrets.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
BLERemoteCharacteristic* legoControl = nullptr;

void scanLEGOHub()
{
    Serial.println("Scanning for LEGO Hub...");

    BLEDevice::init("");
    BLEScan* scanner = BLEDevice::getScan();
    scanner->setActiveScan(true);

    BLEScanResults* results = scanner->start(5, false);

    for (int i = 0; i < results->getCount(); i++)
    {
        BLEAdvertisedDevice device = results->getDevice(i);

        Serial.print("Device: ");
        Serial.println(device.toString().c_str());
    }

    scanner->clearResults();
}
void legoNotificationCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify)
{
    Serial.print("LEGO Hub response: ");

    for (size_t i = 0; i < length; i++)
    {
        Serial.printf("%02X ", data[i]);
    }

    Serial.println();
}
void connectToLEGOHub()
{
    Serial.println("Connecting to LEGO Hub...");

    BLEAddress hubAddress("9c:9a:c0:4a:8b:b9");

    BLEClient* client = BLEDevice::createClient();

    if (client->connect(hubAddress))
    {
        Serial.println("BLE connected to LEGO Hub!");

        BLEUUID serviceUUID(
            "00001623-1212-efde-1623-785feabcd123"
        );

        BLERemoteService* service =
            client->getService(serviceUUID);

        if (service != nullptr)
{
    Serial.println("LEGO service found!");

    BLEUUID characteristicUUID(
        "00001624-1212-efde-1623-785feabcd123"
    );

    BLERemoteCharacteristic* characteristic =
        service->getCharacteristic(characteristicUUID);

    if (characteristic != nullptr)
    {
        Serial.println("LEGO control characteristic found!");

        if (characteristic->canWrite())
        {  legoControl = characteristic;

if (legoControl->canNotify())
{
    legoControl->registerForNotify(
        legoNotificationCallback
    );

    Serial.println("LEGO notifications enabled!");
}
else
{
    Serial.println("LEGO notifications unavailable!");
}

Serial.println("LEGO Hub ready for motor commands!");
        }
        else
        {
            Serial.println("Characteristic is not writable!");
        }
    }
    else
    {
        Serial.println("LEGO control characteristic NOT found!");
    }
}
    }
    else
    {
        Serial.println("BLE connection failed!");
    }
}

void setMotorSpeed(int8_t speed)
{
    if (legoControl == nullptr)
    {
        Serial.println("LEGO Hub not connected!");
        return;
    }

    uint8_t command[] = {
       0x08,            // Message length
        0x00,            // Hub ID
        0x81,            // Port Output Command
        0x00,            // Port A
        0x10,            // Execute immediately
        0x51,            // WriteDirectModeData
        0x00,            // Mode 0: Power
        (uint8_t)speed   // Power percentage
    };

    legoControl->writeValue(command, sizeof(command), false);

    Serial.printf("Motor A power: %d%%\n", speed);
}
void setLights(uint8_t brightness)
{
    if (legoControl == nullptr)
    {
        Serial.println("LEGO Hub not connected!");
        return;
    }

    uint8_t command[] = {
        0x08,        // Message length
        0x00,        // Hub ID
        0x81,        // Port Output Command
        0x01,        // Port B
        0x10,        // Execute immediately
        0x51,        // WriteDirectModeData
        0x00,        // Mode 0
        brightness   // Output value
    };

    legoControl->writeValue(command, sizeof(command), false);

    Serial.printf("Port B output: %d%%\n", brightness);
}
unsigned long motorStopTime = 0;
bool motorRunning = false;

const unsigned long MOTOR_TIMEOUT = 10000;
void setup()
{

    Serial.begin(115200);
    delay(2000);
    scanLEGOHub();
connectToLEGOHub();
    Serial.println("Starting ESP32...");
    int networks = WiFi.scanNetworks();

Serial.println(networks);
for (int i = 0; i < networks; i++) {
  Serial.println(WiFi.SSID(i));
}
    WiFi.begin(ssid, password);

    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("WiFi connection timeout!");
    }
}
void loop()
{
    if (Serial.available() > 0)
    {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toUpperCase();

        if (command == "START")
        {
            Serial.println("Starting Motor A...");

            setMotorSpeed(50);

            motorRunning = true;

            motorStopTime = millis() + MOTOR_TIMEOUT;

            Serial.println("Motor A STOPPED!");
        }
        else if (command == "LIGHTS ON")
{
    setLights(100);
    Serial.println("Lights ON!");
}
else if (command == "LIGHTS OFF")
{
    setLights(0);
    Serial.println("Lights OFF!");
}
else if (command.startsWith("LIGHTS "))
{
    String value = command.substring(7);
    int brightness = value.toInt();

    if (brightness >= 0 && brightness <= 100 &&
        value.length() > 0 &&
        value.toInt() == brightness)
    {
        setLights(brightness);
        Serial.printf("Lights brightness: %d%%\n", brightness);
    }
    else
    {
        Serial.println("Invalid brightness! Use 0-100.");
    }
}
else if (command.startsWith("SPEED "))
{
    String value = command.substring(6);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        setMotorSpeed(speed);

        motorRunning = (speed > 0);
        motorStopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf("Motor power: %d%%\n", speed);
    }
    else
    {
        Serial.println("Invalid speed! Use 0-60.");
    }
}
else if (command == "STOP")
{
    setMotorSpeed(0);
    motorRunning = false;

    Serial.println("Motor STOPPED!");
}
    }
    if (motorRunning &&
    (long)(millis() - motorStopTime) >= 0)
{
    setMotorSpeed(0);
    motorRunning = false;

    Serial.println("Automatic STOP - timeout!");
}
}
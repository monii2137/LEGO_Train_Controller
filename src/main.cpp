
#include <Arduino.h>
#include <WiFi.h>
#include "wifi_secrets.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "UltrasonicSensor.h"
#include "AutomaticBraking.h"



const unsigned long ACCEL_INTERVAL = 200;
const int ACCEL_STEP = 5;

const unsigned long BRAKE_INTERVAL = 200;
const int BRAKE_STEP = 5;

const int SMOOTH_START_SPEED = 30;


//for tests 
unsigned long lastDistanceMeasurement = 0;
const unsigned long DISTANCE_INTERVAL = 100;


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



class Train
{
public:
    BLEClient* TrainClient = nullptr;
    BLERemoteCharacteristic* TrainControl = nullptr;

    const int trainID = 1;
    int TrainSpeed = 0;
    bool TrainRunning = false;
    bool TrainBraking = false;
    bool TrainAccelerating = false;
    unsigned long TrainBrakeTime = 0;
    int TrainTargetSpeed = 0;
    unsigned long TrainAccelTime = 0;

    Train(const int ID) : trainID(ID) {}

    void setMotorSpeed(int8_t speed)
    {
        if (TrainControl == nullptr)
        {
            Serial.println("Train control not available!");
            return;
        }

        if (TrainClient == nullptr)
        {
            Serial.println("Train client not available!");
            return;
        }

        // Check BLE connection
        if (!TrainClient->isConnected())
        {
            Serial.println("BLE disconnected! Motor command not sent!");
            return;
        }

        // LEGO Powered Up motor command
        uint8_t command[] = 
        {
            0x08,
            0x00,
            0x81,
            0x00,
            0x10,
            0x51,
            0x00,
            (uint8_t)speed
        };

        // Send command to LEGO Hub
        TrainControl->writeValue(command, sizeof(command), false);

        // Remember last commanded motor power
        TrainSpeed = speed;
        TrainRunning = (speed != 0);
    }

    // -
    
    void smoothStart(int targetSpeed)
    {
        if (TrainControl == nullptr)
        {
            Serial.println("Train not connected!");
            return;
        }

  
        if (isObstacleDetected(trainID))
            return;

        TrainBraking = false;
        TrainTargetSpeed = targetSpeed;
        TrainAccelerating = (TrainSpeed != targetSpeed);
        TrainAccelTime = millis();
        TrainRunning = (targetSpeed != 0 || TrainSpeed != 0);
    }

    // -

    void smoothStop()
    {
        if (TrainControl == nullptr)
        {
            Serial.println("Train not connected!");
            return;
        }

       
        TrainAccelerating = false;
        TrainBraking = (TrainSpeed != 0);
        TrainBrakeTime = millis();

        if (!TrainBraking)
        {
            TrainRunning = false;
        }
    }

    // -

    void reverse()
    {
        TrainAccelerating = false;
        TrainBraking = false;
        setMotorSpeed(0);
        delay(500);
        setMotorSpeed(-20);
    }

    // -

    void updateAcceleration()
    {
        if (TrainControl == nullptr)
        {
            Serial.println("Train control not available!");
            return;
        }

        unsigned long now = millis();

        if (TrainAccelerating && now - TrainAccelTime >= ACCEL_INTERVAL)
        {
            TrainAccelTime = now;

            int speed = TrainSpeed;

            if (speed < TrainTargetSpeed)
            {
                speed = min(speed + ACCEL_STEP, TrainTargetSpeed);
            }
            else
            {
                speed = max(speed - ACCEL_STEP, TrainTargetSpeed);
            }

            setMotorSpeed(speed);

            if (speed == TrainTargetSpeed)
            {
                TrainAccelerating = false;
                Serial.printf("Train %d reached target power!\n", trainID);
            }
        }

    }

    // -

    void updateBraking()
    {
        unsigned long now = millis();

        if (TrainBraking && now - TrainBrakeTime >= BRAKE_INTERVAL)
        {
            TrainBrakeTime = now;

            int speed = TrainSpeed;

            if (speed > 0)
            {
                speed = max(0, speed - BRAKE_STEP);
            }
            else
            {
                speed = min(0, speed + BRAKE_STEP);
            }

            setMotorSpeed(speed);

            if (speed == 0)
            {
                TrainBraking = false;
                TrainRunning = false;
                Serial.printf("Train %d smoothly stopped!\n", trainID);
            }
        }
    }

    // -

    void setLights(uint8_t brightness){

        if (TrainControl == nullptr) 
        {
            Serial.println("Train not connected!");
            return;
        }

        uint8_t command[] = {
            0x08,
            0x00,
            0x81,
            0x01,
            0x10,
            0x51,
            0x00,
            brightness
        };

        TrainControl->writeValue(command, sizeof(command), false);
    }

    // -

    void cleanup()
    {
        TrainControl = nullptr;

        if (TrainClient != nullptr && TrainClient->isConnected())
        {
            TrainClient->disconnect();
        }

        TrainSpeed = 0;
        TrainRunning = false;
        TrainBraking = false;
        TrainAccelerating = false;
        TrainBrakeTime = 0;
        TrainTargetSpeed = 0;
        TrainAccelTime = 0;
    }

    bool connectToLEGOHub(const char* address)
    {
        if (TrainClient != nullptr && TrainClient->isConnected() && TrainControl != nullptr)
        {
            Serial.println("Train already connected!");
            return false;
        }

        cleanup();

        Serial.print("Connecting to LEGO Hub: ");
        Serial.println(address);

        if (TrainClient == nullptr)
        {
            TrainClient = BLEDevice::createClient();
        }

        if (TrainClient == nullptr)
        {
            Serial.println("Failed to create BLE client!");
            return false;
        }

        BLEAddress hubAddress(address);

        if (!TrainClient->connect(hubAddress)) 
        {
            Serial.println("BLE connection failed!");
            cleanup();
            return false;
        }

        Serial.println("BLE connected!");

        BLEUUID serviceUUID("00001623-1212-efde-1623-785feabcd123");

        BLERemoteService* service = TrainClient->getService(serviceUUID);

        if (service == nullptr) 
        {
            Serial.println("LEGO service not found!");
            cleanup();
            return false;
        }

        BLEUUID characteristicUUID("00001624-1212-efde-1623-785feabcd123");

        BLERemoteCharacteristic* characteristic = service->getCharacteristic(characteristicUUID);

        if (characteristic == nullptr || !characteristic->canWrite()) 
        {
            Serial.println("LEGO control not available!");
            cleanup();
            return false;
        }

        TrainControl = characteristic;

        if (TrainControl->canNotify()) 
        {
            TrainControl->registerForNotify(legoNotificationCallback);
        }

        Serial.println("LEGO Hub ready!");
        return true;
    }



    // -
};


constexpr size_t trainAmount = 1; // max 3
constexpr char* MacAddresses[3] = {"9c:9a:c0:4a:8b:b9", "9c:9a:c0:33:9f:eb","9c:9a:c0:4b:fd:20"};
Train* trains[trainAmount] = {};


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



void InitSerial()
{
    Serial.begin(115200);
    unsigned long serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < 5000)
    {
        delay(10);
    }

    Serial.println("Starting setup...");
}

void InitUltrasonic()
{
    if (SENSOR_CONNECTED)
    {
        Serial.println("Initializing ultrasonic sensor...");
        initUltrasonicSensor();
    }
    else
    {
        Serial.println("Ultrasonic sensor disabled; obstacle checks are bypassed.");
    }

    Serial.println("Initializing automatic braking...");
    initializeAutomaticBraking();
}

void ConnectTrains()
{
    Serial.println("Starting LEGO Hub scan...");
    scanLEGOHub();

    for(size_t i = 0; i < trainAmount; i++)
    {
        trains[i] = new Train(static_cast<int>(i + 1));
        if (trains[i] == nullptr)
        {
            Serial.printf("Failed to create Train %u!\n", static_cast<unsigned>(i + 1));
            continue;
        }

        trains[i]->connectToLEGOHub(MacAddresses[i]);
    }

}

void InitWifi()
{
    Serial.println("Starting Wifi...");

    int networks = WiFi.scanNetworks();

    Serial.println(networks);
    for (int i = 0; i < networks; i++) 
    {
        Serial.println(WiFi.SSID(i));
    }
    WiFi.begin(ssid, password);

    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
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

void setup()
{
    InitSerial();

    initUltrasonicSensor();

    ConnectTrains();

    InitWifi();
}

struct ParsedCommand
{
    String core;
    String args;
};

ParsedCommand parseCommand(const String& input)
{
    String text = input;
    text.trim();

    ParsedCommand result;
    int separator = text.indexOf(' ');

    if (separator < 0)
    {
        result.core = text;
    }
    else
    {
        result.core = text.substring(0, separator);
        result.args = text.substring(separator + 1);
        result.args.trim();
    }

    result.core.toUpperCase();
    return result;
}

bool splitFirstArgument(const String& input, String& first, String& remainder)
{
    String text = input;
    text.trim();

    const int separator = text.indexOf(' ');
    if (separator <= 0 || separator == static_cast<int>(text.length()) - 1)
    {
        return false;
    }

    first = text.substring(0, separator);
    first.trim();
    remainder = text.substring(separator + 1);
    remainder.trim();
    return first.length() > 0 && remainder.length() > 0;
}

bool parseTrainID(const String& target, int& trainID)
{
    if (target.length() == 0)
    {
        Serial.println("Train target is required.");
        return false;
    }

    unsigned int parsedID = 0;
    for (size_t i = 0; i < target.length(); ++i)
    {
        const char digit = target.charAt(i);
        if (digit < '0' || digit > '9')
        {
            Serial.println("Train target must be a train number or ALL.");
            return false;
        }

        parsedID = parsedID * 10 + static_cast<unsigned int>(digit - '0');
        if (parsedID > trainAmount)
        {
            break;
        }
    }

    if (parsedID < 1 || parsedID > trainAmount)
    {
        Serial.printf("Train number must be between 1 and %u!\n", static_cast<unsigned>(trainAmount));
        return false;
    }

    trainID = static_cast<int>(parsedID);
    return true;
}

Train* getCommandTrain(int trainID)
{
    if (trainID < 1 || static_cast<size_t>(trainID) > trainAmount)
    {
        return nullptr;
    }

    Train* train = trains[trainID - 1];
    if (train == nullptr)
    {
        Serial.printf("Train %d is not initialized!\n", trainID);
        return nullptr;
    }

    if (train->TrainControl == nullptr || train->TrainClient == nullptr || !train->TrainClient->isConnected())
    {
        Serial.printf("Train %d control not available!\n", trainID);
        return nullptr;
    }

    return train;
}

template <typename Action>
void forEachTargetTrain(const String& requestedTarget, Action action)
{
    String target = requestedTarget;
    target.trim();
    target.toUpperCase();

    if (target == "ALL")
    {
        for (size_t i = 0; i < trainAmount; ++i)
        {
            Train* train = getCommandTrain(static_cast<int>(i + 1));
            if (train != nullptr)
            {
                action(*train);
            }
        }
        return;
    }

    int trainID = 0;
    if (!parseTrainID(target, trainID))
    {
        return;
    }

    Train* train = getCommandTrain(trainID);
    if (train != nullptr)
    {
        action(*train);
    }
}

bool parseBoundedValue(const String& text, int maximum, int& value)
{
    if (text.length() == 0 || text.length() > 3)
    {
        return false;
    }

    int parsedValue = 0;
    for (size_t i = 0; i < text.length(); ++i)
    {
        const char digit = text.charAt(i);
        if (digit < '0' || digit > '9')
        {
            return false;
        }

        parsedValue = parsedValue * 10 + (digit - '0');
        if (parsedValue > maximum)
        {
            return false;
        }
    }

    value = parsedValue;
    return true;
}

void HandleCommands()
{
    if (Serial.available() <= 0)
    {
        return;
    }

    ParsedCommand command = parseCommand(Serial.readStringUntil('\n'));
    if (command.core.length() == 0)
    {
        return;
    }

    if (command.core == "START")
    {
        const String target = command.args.length() == 0 ? "ALL" : command.args;

        forEachTargetTrain(target, [](Train& train)
        {
            if (isObstacleDetected(train.trainID))
            {
                Serial.printf("Train %d START blocked!\n", train.trainID);
                return;
            }

            train.smoothStart(SMOOTH_START_SPEED);
            Serial.printf("Train %d starting!\n", train.trainID);
        });
    }
    else if (command.core == "STOP")
    {
        const String target = command.args.length() == 0 ? "ALL" : command.args;
        forEachTargetTrain(target, [](Train& train)
        {
            train.smoothStop();
            Serial.printf("Train %d stopping!\n", train.trainID);
        });
    }
    else if (command.core == "SPEED")
    {
        String target;
        String valueText;
        int speed = 0;
        if (!splitFirstArgument(command.args, target, valueText) || !parseBoundedValue(valueText, 70, speed))
        {
            Serial.println("Usage: SPEED <train number|ALL> <0-70>");
            return;
        }

        forEachTargetTrain(target, [speed](Train& train)
        {
            if (speed > 0 && isObstacleDetected(train.trainID))
            {
                Serial.printf("Train %d SPEED blocked! Obstacle detected.\n", train.trainID);
                return;
            }

            train.TrainAccelerating = false;
            train.TrainBraking = false;
            train.setMotorSpeed(speed);
            Serial.printf("Train %d motor power: %d%%\n", train.trainID, speed);
        });
    }
    else if (command.core == "REVERSE")
    {
        forEachTargetTrain(command.args, [](Train& train)
        {
            if (isObstacleDetected(train.trainID))
            {
                Serial.printf("Train %d REVERSE blocked! Obstacle detected.\n", train.trainID);
                return;
            }

            train.reverse();
            Serial.printf("Train %d reversing!\n", train.trainID);
        });
    }
    else if (command.core == "LIGHTS")
    {
        String target;
        String valueText;
        if (!splitFirstArgument(command.args, target, valueText))
        {
            Serial.println("Usage: LIGHTS <train number|ALL> <ON|OFF|0-100>");
            return;
        }

        valueText.toUpperCase();
        int brightness = 0;
        if (valueText == "ON")
        {
            brightness = 100;
        }
        else if (valueText == "OFF")
        {
            brightness = 0;
        }
        else if (!parseBoundedValue(valueText, 100, brightness))
        {
            Serial.println("Invalid brightness! Use ON, OFF, or 0-100.");
            return;
        }

        forEachTargetTrain(target, [brightness](Train& train)
        {
            train.setLights(static_cast<uint8_t>(brightness));
            Serial.printf("Train %d lights: %d%%\n", train.trainID, brightness);
        });
    }
    else
    {
        Serial.printf("Unknown command: %s\n", command.core.c_str());
    }
}

void loop()
{
    HandleCommands();

    if (SENSOR_CONNECTED && millis() - lastDistanceMeasurement >= DISTANCE_INTERVAL)
    {
        lastDistanceMeasurement = millis();
        updateAutomaticBraking(1, measureDistance());

        for (size_t i = 0; i < trainAmount; ++i)
        {
            Train* train = trains[i];
            if (train != nullptr &&
                isObstacleDetected(train->trainID) &&
                train->TrainRunning &&
                !train->TrainBraking)
            {
                train->smoothStop();
                Serial.printf("Train %d - Automatic braking!\n", train->trainID);
            }
        }
    }

    for (size_t i = 0; i < trainAmount; ++i)
    {
        if (trains[i] != nullptr)
        {
            trains[i]->updateAcceleration();
            trains[i]->updateBraking();
        }
    }
}

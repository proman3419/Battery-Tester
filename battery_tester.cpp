#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <DS1307.h>

// Channels
#define D0 0
#define D1 1
#define D2 2
#define D3 3
#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define D8 8
#define D9 9
#define D10 10
#define D11 11
#define D12 12
#define D13 13
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
#define TX D0
#define RX D1
#define ONEWIRE D2
#define MULTIPLEXER_CONTROL_CH D13
#define ONE_WIRE_CH ONEWIRE
#define FIRST_CHARGING_CH D3
#define FIRST_DISCHARGING_CH D4

// Constants
#define TEMPERATURE_PRECISION 10 //bits
#define MULTIPLEXER_BITS 1
#define MULTIPLEXER_VARIANTS 1
#define CHARGED_VOLTAGE 95
#define DISCHARGED_VOLTAGE 15
#define SETTLE_VOLTAGE 70
#define OVERHEAT_TEMPERATURE 70
#define SLOTS_AMOUNT 4
#define MAX_OVERHEATED 5
#define ADC_RESOLUTION 1024
#define ADC_VOLTAGE_RESOLUTION 5
#define ALARM_TIME_OFFSET_HOURS 0
#define ALARM_TIME_OFFSET_MINUTES 5
#define ALARM_TIME_OFFSET_SECONDS 0

typedef enum {_DEFAULT, IDLE, TESTED, CHARGING, DISCHARGING, OVERHEATED}
BATTERY_STATE;

typedef struct
{
    BATTERY_STATE previousState;
    BATTERY_STATE nextState;
    float voltage;
    float temperature;
    unsigned int cycleFinished :1;
    unsigned int overheated :2;
    RTCDateTime alarmTime;
} Battery;

void setupPins();
void setupSensors();
void setupRTC();
void setMultiplexerPin();
void _default();
void idle();
void tested();
void charging();
void discharging();
void overheated();
void testBattery(const char *message);
void startCharging();
void stopCharging();
void startDischarging();
void stopDischarging();
void measureVoltage();
void measureTemperature();
uint32_t toUnixTimeHMS(int hours, int minutes, int seconds);
void addOffsetToRTCDateTime(RTCDateTime &dt);
int compareRTCDateTime(const RTCDateTime &a, const RTCDateTime &b); // returns -1 if a < b, 1 if a > b, 0 if a == b 
void logToRasberry(const char *message);
void logToRasberryVoltage();
void logToRasberryTemperature();

Battery batteries[SLOTS_AMOUNT * MULTIPLEXER_VARIANTS];
Battery *currBattery;
BATTERY_STATE currState;
OneWire oneWire(ONE_WIRE_CH);
DallasTemperature sensors(&oneWire);
DeviceAddress thermometers[SLOTS_AMOUNT];
RTCDateTime currTime;
RTCDateTime alarmTimeOffset;
DS1307 clock;
int n = 0, m = 0, x = 0; // x - battery slot index, n - analog channel, m - multiplexer channel

void setup()
{    
    Serial.begin(9600);
    setupPins();
    setupSensors();
    setupRTC();
}

void loop() 
{
    for (n = 0; n < SLOTS_AMOUNT; n++)
    {
        for (m = 0; m < MULTIPLEXER_VARIANTS; m++)
        {
            setMultiplexerPin();
            x = (m + n*MULTIPLEXER_VARIANTS);
            currBattery = &batteries[x];
            currState = currBattery->nextState;
            switch (currState)
            {
                case _DEFAULT: // State after inserting a new battery/reseting the device
                    _default(); break;
                case IDLE:
                    idle(); break;
                case TESTED:
                    tested(); break;
                case CHARGING:
                    charging(); break;
                case DISCHARGING:
                    discharging(); break;
                case OVERHEATED:
                    overheated(); break;
                default:
                    currBattery->nextState = _DEFAULT; break;
            }
            currBattery->previousState = currState;
        }
    }
}

void setupPins()
{
    Serial.println("Setting up pins");
    
    for (int i = D2; i <= D13; i++)
        pinMode(i, OUTPUT);
}

void setupSensors()
{
    Serial.println("Setting up sensors");
    
    sensors.begin();
    for (int i = 0; i < SLOTS_AMOUNT; i++)
    {
        if (!sensors.getAddress(thermometers[i], i))
        {
            Serial.print("Unable to find address for thermometer ");
            Serial.println(i);
            continue;
        }
        sensors.setResolution(thermometers[i], TEMPERATURE_PRECISION);
    }
}

void setupRTC()
{
    Serial.println("Setting up RTC");
    
    clock.begin();
    if (!clock.isReady())
        clock.setDateTime(__DATE__, __TIME__);

    alarmTimeOffset.hour = ALARM_TIME_OFFSET_HOURS;
    alarmTimeOffset.minute = ALARM_TIME_OFFSET_MINUTES;
    alarmTimeOffset.second = ALARM_TIME_OFFSET_SECONDS;
    alarmTimeOffset.unixtime = toUnixTimeHMS(ALARM_TIME_OFFSET_HOURS, ALARM_TIME_OFFSET_MINUTES, ALARM_TIME_OFFSET_SECONDS);
}

void setMultiplexerPin()
{
    for (int i = 1; i <= MULTIPLEXER_BITS; i++)
    {
        if (m % (int)pow(2, i) == 1)
            digitalWrite(MULTIPLEXER_CONTROL_CH + MULTIPLEXER_BITS - i, HIGH);
        else
            digitalWrite(MULTIPLEXER_CONTROL_CH + MULTIPLEXER_BITS - i, LOW);
    }
}

void _default()
{
    *currBattery = {_DEFAULT, _DEFAULT, 0.0, 0.0, 0, 0};
    testBattery("Default");

    if (currBattery->voltage > CHARGED_VOLTAGE)
        currBattery->nextState = IDLE;
    else
        currBattery->nextState = CHARGING;
}

void idle()
{
    testBattery("Idle");
    
    currTime = clock.getDateTime();
    currBattery->alarmTime = currTime;
    addOffsetToRTCDateTime(currBattery->alarmTime);
    currBattery->previousState = IDLE;

    if ((compareRTCDateTime(currBattery->alarmTime, currTime) == 1) && (currBattery->voltage >= CHARGED_VOLTAGE * 0.9))
        currBattery->nextState = CHARGING;
}

void tested()
{
    if (currBattery->previousState != IDLE)
        testBattery("Tested");
}

void charging()
{
    testBattery("Charging");

    if (currBattery->temperature >= OVERHEAT_TEMPERATURE)
    {
        stopCharging();
        currBattery->nextState = OVERHEATED;
    } 
    else
    {
        if (currBattery->previousState != CHARGING)
            startCharging();
        
        if (currBattery->cycleFinished == true && currBattery->voltage >= SETTLE_VOLTAGE)
        {
            currBattery->nextState = TESTED;
            stopCharging();
        }    
        else if (currBattery->cycleFinished == false && currBattery->voltage >= CHARGED_VOLTAGE)
        {
            currBattery->nextState = IDLE;
            stopCharging();
        }
    }
}

void discharging()
{
    testBattery("Discharging");
    
    if (currBattery->previousState != DISCHARGING)
        startDischarging();
    if (currBattery->voltage <= DISCHARGED_VOLTAGE)
    {
        currBattery->nextState = CHARGING;
        currBattery->cycleFinished = true;
        stopDischarging();
    }
}

void overheated()
{
    measureTemperature();
    logToRasberryTemperature();
    currBattery->overheated++;
    if (currBattery->overheated > MAX_OVERHEATED)
    {
        currBattery->nextState = TESTED;
        testBattery("Broken");
    }
    else
        currBattery->nextState = IDLE;
}

void measureVoltage()
{
    int sumVoltage = 0;
    for (int i = 0; i < 5; i++)
        sumVoltage += analogRead(A0 + n); // A0 - first analog pin
    currBattery->voltage = sumVoltage/5 * ADC_VOLTAGE_RESOLUTION/ADC_RESOLUTION;
}

void measureTemperature()
{
    float sumTemperature;
    for(int i = 0; i < 5; i++)
        sumTemperature += sensors.getTempC(thermometers[n]);
    currBattery->temperature = sumTemperature / 5;
}

void testBattery(const char *message)
{
    measureVoltage();
    measureTemperature();
    logToRasberry(message);
    logToRasberryVoltage();
    logToRasberryTemperature();
}

void startCharging()
{
    digitalWrite(FIRST_DISCHARGING_CH + 2*n, LOW);
    digitalWrite(FIRST_CHARGING_CH + 2*n, HIGH);
}

void stopCharging()
{
    digitalWrite(FIRST_CHARGING_CH + 2*n, LOW);
    digitalWrite(FIRST_DISCHARGING_CH + 2*n, LOW);
}

void startDischarging()
{
    digitalWrite(FIRST_CHARGING_CH + 2*n, LOW);
    digitalWrite(FIRST_DISCHARGING_CH + 2*n, HIGH);
}

void stopDischarging()
{
    digitalWrite(FIRST_CHARGING_CH + 2*n, LOW);
    digitalWrite(FIRST_DISCHARGING_CH + 2*n, LOW);
}

uint32_t toUnixTimeHMS(int hours, int minutes, int seconds)
{
    uint32_t u;
    u = ((hours*60) + minutes)*60 + seconds;
    u += 946681200;
    return u;
}

void addOffsetToRTCDateTime(RTCDateTime &dt)
{
    dt.hour += alarmTimeOffset.hour;
    dt.minute += alarmTimeOffset.minute;
    dt.second += alarmTimeOffset.second;    
    dt.unixtime += alarmTimeOffset.unixtime;
}

int compareRTCDateTime(const RTCDateTime &a, const RTCDateTime &b)
{
    if (a.unixtime < b.unixtime)
        return -1;
    if (a.unixtime > b.unixtime)
        return 1;
    if (a.unixtime == b.unixtime)
        return 0;
}

void logToRasberry(const char *message)
{
    char *s;
    sprintf(s, "Battery #%d %s", x, message);
    Serial.println(s);
}

void logToRasberryVoltage()
{
    char *s;
    sprintf(s, "Battery #%d voltage %d", x, currBattery->voltage);
    Serial.println(s);
}

void logToRasberryTemperature()
{
    char *s;
    sprintf(s, "Battery #%d temperature %d", x, currBattery->voltage);
    Serial.println(s);
}

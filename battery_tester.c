#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <virtualbotixRTC.h>

// Channels
#define TX 0
#define RX 1
#define ONEWIRE 2
#define D1 3
#define D2 4
#define D3 5
#define D4 6
#define D5 7
#define D6 8
#define D7 9
#define D8 10
#define D9 11
#define D10 12
#define D11 13
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
#define MULTIPLEXER_CONTROL_PIN RX
#define ONE_WIRE_BUS ONEWIRE
#define FIRST_CHARGING_PIN D1
#define FIRST_DISCHARGING_PIN D2

// Constants
#define TEMPERATURE_PRECISION 10 //bits
#define MULTIPLEXER_BITS 4
#define MULTIPLEXER_VARIANTS 16
#define CHARGED_VOLTAGE 95
#define DISCHARGED_VOLTAGE 15
#define SETTLE_VOLTAGE 70
#define OVERHEAT_TEMPERATURE 70
#define SLOTS_AMOUNT 4
#define MAX_OVERHEATED 5
#define ADC_RESOLUTION 1024
#define ADC_VOLTAGE_RESOLUTION 5


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
  //RTC_TIME alarmTime
} Battery;

void setupPins();
void setupSensors();
void setMultiplexerPin();
void _default();
void idle();
void tested();
void charging();
void discharging();
void overheated();
float measureVoltage();
float measureTemperature();
void testBattery();
void stopCharging();
void startCharging();
void logToRasberry(char *message);
void logToRasberryVoltage();
void logToRasberryTemperature();
void manageOverheat();

Battery batteries[SLOTS_AMOUNT * MULTIPLEXER_VARIANTS];
Battery *currBattery;
BATTERY_STATE currState;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress thermometers[SLOTS_AMOUNT];
int n = 0, m = 0, x = 0; // x - battery slot index, n - analog channel, m - multiplexer channel
unsigned int alarmTime; // It has to be a type of RTC lib

void setup()
{
  Serial.begin(9600);
  setupPins();
  setupSensors();
  sensors.begin();
  //initRTC();
}

void loop() {
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
  for (int i = 3; i <= 8; i++)
    pinMode(i, OUTPUT);
}

void setupSensors()
{
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

void setMultiplexerPin()
{
  for (int i = 1; i <= MULTIPLEXER_BITS; i++)
  {
    if (m % (int)pow(2, i) == 1)
      digitalWrite(MULTIPLEXER_CONTROL_PIN + MULTIPLEXER_BITS - i, HIGH);
    else
      digitalWrite(MULTIPLEXER_CONTROL_PIN + MULTIPLEXER_BITS - i, LOW);
  }
}

void _default()
{
  *currBattery = {_DEFAULT, _DEFAULT, 0.0, 0.0, 0, 0}; // Reset battery
  testBattery();

  if (currBattery->voltage > CHARGED_VOLTAGE)
    currBattery->nextState = IDLE;
  else
    currBattery->nextState = CHARGING;
}

void idle()
{
  alarmTime = 0; //get current clock + 5min; // alarmTime requires RTC lib varaible type
  currBattery->previousState = IDLE;
  //if (AlarmTime == (getDate()) && measureVoltage(x) >= (CHARGED_VOLTAGE * 0.9)) // getDate() requires RTC lib
    currBattery->nextState = CHARGING;
  //testBattery(); // write to whole structure
  logToRasberryVoltage();
}

void tested()
{
  if (currBattery->previousState != IDLE)
    logToRasberry("Tested");
}

void charging()
{
  testBattery();
  logToRasberry("Charging");
  logToRasberryVoltage();
  logToRasberryTemperature();

  if (currBattery->previousState != CHARGING)
    startCharging();
  if (measureTemperature() >= OVERHEAT_TEMPERATURE)
  {
    stopCharging();
    currBattery->overheated = true;
    currBattery->nextState = OVERHEATED;
  }

  if (currBattery->cycleFinished == true && currBattery->voltage >= SETTLE_VOLTAGE)
    currBattery->nextState = TESTED;
  else if (currBattery->cycleFinished == false && currBattery->voltage >= CHARGED_VOLTAGE)
    currBattery->nextState = IDLE;
}

void discharging()
{
  //if (currBattery->previousState != DISCHARGING)
    //startDischarging();

  logToRasberryVoltage();
  logToRasberryTemperature();
  currBattery->previousState = DISCHARGING;

  if (measureVoltage() <= DISCHARGED_VOLTAGE)
  {
    currBattery->nextState = CHARGING;
    currBattery->cycleFinished = true;
  }
}

void overheated()
{
  currBattery->previousState = OVERHEATED;
  measureTemperature();
  logToRasberryTemperature();
  //manageOverheat();
  // IDLE or CHARGE/DISCHARGE next
}

float measureVoltage()
{
  int sumVoltage = 0;
  for (int i = 0; i < 5; i++)
    sumVoltage += analogRead(A0 + n); // A0 - first analog pin
  return (sumVoltage/5 * ADC_VOLTAGE_RESOLUTION/ADC_RESOLUTION);
}

float measureTemperature()
{
  float sumTemperature;
  for(int i = 0; i < 5; i++)
    sumTemperature += sensors.getTempC(thermometers[n]);
  return sumTemperature / 5;
}

void testBattery()
{
  currBattery->voltage = measureVoltage();
  currBattery->temperature = measureTemperature();
}

void startCharging()
{
  digitalWrite(FIRST_CHARGING_PIN + n, HIGH);
  digitalWrite(FIRST_DISCHARGING_PIN + n, LOW);
}

void stopCharging()
{
  digitalWrite(FIRST_CHARGING_PIN + n, LOW);
  digitalWrite(FIRST_DISCHARGING_PIN + n, HIGH);
}

void logToRasberry(char *message)
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

void manageOverheat()
{

}

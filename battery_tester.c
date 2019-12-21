#include <stdbool.h>
#include <stdio.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <virtualbotixRTC.h>

#define ONE_WIRE_BUS 2 //pin
#define TEMPERATURE_PRECISION 10 //bits
#define SLOTS_AMOUNT 4
#define MULTIPLEXER_MUTLTIPLIER 16
#define CHARGED_VOLTAGE 95
#define DISCHARGED_VOLTAGE 15
#define SETTLE_VOLTAGE 70
#define OVERHEAT_TEMPERATURE 70
#define BATTERY_AMOUNT 6
#define MAX_OVERHEATED 5

typedef enum {_DEFAULT, IDLE, TESTED, CHARGING, DISCHARGING, OVERHEATED}
BATTERY_STATE;

typedef struct
{
  BATTERY_STATE previousState;
  BATTERY_STATE nextState;
  float voltage;
  float temperature;
  unsigned int cycleFinnished :1;
  unsigned int overheated :2;
  //RTC_TIME alarmTime
} Battery;

void _default(int n, Battery *currBattery, Battery batteries[]);
void idle(int alarmTime, int x, Battery *currBattery, Battery batteries[]);
void tested(int x, Battery *currBattery);
void charging(int x, Battery *currBattery, Battery batteries[]);
void discharging(int x, Battery *currBattery);
void overheated(int x, Battery *currBattery);
float measureVoltage(int n);
float measureTemperature(int n);
void testBattery(int n, Battery *currBattery);
void stopChargging(int x);
void startCharging(int x);
void logToRasberry(int x);
void logToRasberryVoltage(int x);
void logToRasberryTemperature(int x);
void manageOverheat(int x);

Battery batteries[SLOTS_AMOUNT * MULTIPLEXER_MUTLTIPLIER];
Battery *currBattery;
int n = 0, m = 0, x = 0; // x - battery slot index, n - analog channel, m - multiplexer channel
unsigned int alarmTime; // It has to be a type of RTC lib

void setup()
{
  Serial.begin(9600);
  //sensors.begin();
  //begin();
  //initRTC();
}

void loop() {
  for (n = 0; n < BATTERY_AMOUNT; n++)
  {
    for (m = 0; m < MULTIPLEXER_MUTLTIPLIER; m++)
    {
      x = (m + n*MULTIPLEXER_MUTLTIPLIER); 
      currBattery = &batteries[x];
      switch (currBattery->nextState)
      {
        case _DEFAULT: // State after inserting a new battery/reseting the device
          _default(n, currBattery, batteries); break;
        case IDLE:
          idle(alarmTime, x, currBattery, batteries); break;
        case TESTED:
          tested(x, currBattery); break;
        case CHARGING:
          charging(x, currBattery, batteries); break;
        case DISCHARGING:
          discharging(x, currBattery); break;
        case OVERHEATED:
          overheated(x, currBattery); break;
        default:
          currBattery->nextState = _DEFAULT; break;
      }
    }
  }
}

void _default(int n, Battery *currBattery, Battery batteries[])
{
  *currBattery = {_DEFAULT, _DEFAULT, 0.0, 0.0, 0, 0};
  testBattery(n, currBattery);

  if (currBattery->voltage > CHARGED_VOLTAGE)
    currBattery->nextState = IDLE;
  else
    currBattery->nextState = CHARGING;
}

void idle(int alarmTime, int x, Battery *currBattery, Battery batteries[])
{
  alarmTime = 0; //get current clock + 5min; // alarmTime requires RTC lib varaible type
  currBattery->previousState = IDLE;
  //if (AlarmTime == (getDate()) && measureVoltage(x) >= (CHARGED_VOLTAGE * 0.9)) // getDate() requires RTC lib
    currBattery->nextState = CHARGING;
  //testBattery(x, batteries[x]); // write to whole structure
  logToRasberryVoltage(x);
}

void tested(int x, Battery *currBattery)
{
  if (currBattery->previousState != IDLE)
    logToRasberry(x);
}

void charging(int x, Battery *currBattery, Battery batteries[])
{
  if (measureTemperature(x) >= OVERHEAT_TEMPERATURE)
  {
    stopChargging(x);
    currBattery->overheated = true;
    currBattery->nextState = OVERHEATED;
  }

  if (currBattery->previousState != CHARGING)
    startCharging(x);

  logToRasberryVoltage(x);
  logToRasberryTemperature(x);
  currBattery->previousState = CHARGING;
  currBattery->voltage = measureVoltage(x);

  if (currBattery->cycleFinnished == true && batteries[x].voltage >= SETTLE_VOLTAGE)
    currBattery->nextState = TESTED;
  else if (currBattery->cycleFinnished == false && batteries[x].voltage >= CHARGED_VOLTAGE)
    currBattery->nextState = IDLE;
}

void discharging(int x, Battery *currBattery)
{
  //if (currBattery->previousState != DISCHARGING)
    //startDischarging(x);

  logToRasberryVoltage(x);
  logToRasberryTemperature(x);
  currBattery->previousState = DISCHARGING;

  if (measureVoltage(x) <= DISCHARGED_VOLTAGE)
  {
    currBattery->nextState = CHARGING;
    currBattery->cycleFinnished = true;
  }
}

void overheated(int x, Battery *currBattery)
{
  currBattery->previousState = OVERHEATED;
  measureTemperature(x);
  logToRasberryTemperature(x);
  //manageOverheat(x);
  // IDLE or CHARGE/DISCHARGE next
}

float measureVoltage(int n)
{
  int sumVoltage = 0;
  for (int i = 0; i < 5; i++)
    sumVoltage += analogRead(A0 + n); // A0 - first analog pin
  return (sumVoltage / 5 * 5 / 1024); 
}

float measureTemperature(int n) 
{
  float sumTemperature;
  //for(int i = 0; i < 5; i++)
  //  sumTemperature += sensors.getTempC(deviceAddress);
  return sumTemperature / 5;
  
}

void testBattery(int n, Battery *currBattery)
{
  currBattery->voltage = measureVoltage(n);
  currBattery->temperature = measureTemperature(n);
}

void startCharging(int x)
{
  pinMode(13, OUTPUT);
  pinMode(1, OUTPUT);
  digitalWrite(13, HIGH); // Pin for charging
  digitalWrite(1, LOW); // Pin for dischraging
}

void stopChargging(int x)
{

}


void logToRasberry(int x)
{

}

void logToRasberryVoltage(int x)
{

}

void logToRasberryTemperature(int x)
{

}

void manageOverheat(int x)
{

}
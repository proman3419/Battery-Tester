//Example program to measure temperature with four thermometers

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2 //pin in witch thermometers are connect
#define TEMPERATURE_PRECISION 10 //resolution in bits

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress temp1, temp2, temp3, temp4;

void setup(void)
{
    Serial.begin(9600); //serial port, serial comunication
    Serial.print("Locating devices...");
    sensors.begin();
    Serial.print("Found ");
    Serial.print(sensors.getDeviceCount(), DEC); //amount of devices
    Serial.println(" devices.");
 
    if (!sensors.getAddress(temp1, 0)) Serial.println("Unable to find address for Device 0");    //error of devices
    if (!sensors.getAddress(temp2, 1)) Serial.println("Unable to find address for Device 1"); 
    if (!sensors.getAddress(temp3, 2)) Serial.println("Unable to find address for Device 2");
    if (!sensors.getAddress(temp4, 3)) Serial.println("Unable to find address for Device 3");

    sensors.setResolution(temp1, TEMPERATURE_PRECISION); // set the resolution for temp1 thermometer
    sensors.setResolution(temp2, TEMPERATURE_PRECISION);
    sensors.setResolution(temp3, TEMPERATURE_PRECISION);
    sensors.setResolution(temp4, TEMPERATURE_PRECISION);
}

// print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
    float tempC = sensors.getTempC(deviceAddress);
    Serial.println(tempC);
}

// print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16) Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void loop(void)
{ 
    delay(200);
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.print("Temp1: ");
    printTemperature(temp1);
    Serial.print("Temp2: ");
    printTemperature(temp2);
    Serial.print("Temp3: ");
    printTemperature(temp3);
    Serial.print("Temp4: ");
    printTemperature(temp4);
}

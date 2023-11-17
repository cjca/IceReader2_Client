/*

All Pin Labels are GPIO Numbers
Pin 0 = RX (Serial1 Input)
Pin 1 = TX (Serial1 Output)
Pin 20 = I2C SDA (Pull Up with 5K Resistor)
Pin 21 = I2C SCL (Pull Up with 5K Resistor)

Pin 5 = GPIO [DHT11 / Air Temp & Humidity]
Pin 6 = GPIO [DS18 / Ice Temp]

Pin 9 = A7 for Battery Voltage Resistor Divider Circuit

Pin 10 = GPIO
Pin 11 = GPIO
Pin 12 = GPIO

Pin 13 = Red LED @ USB Port

A0/14 = IO True Analog Output

A1/15 = Analog GPIO
A2/16 = Analog GPIO
A3/17 = Analog GPIO
A4/18 = Analog GPIO
A5/19 = Analog GPIO

Pin 24 = SPI SCK
Pin 23 = SPI MOSI
Pin 22 = SPI MISO

-------------------------------------------------------------------------
NeoPixel
--------
--> PIN 5
0.1 uF Capacitor between + and Ground of NeoPixel
400 Ohm Resistor between Data Line and NeoPixel (Close to Pixel)

Flat Up
Data In / 5V / Gnd / Data Out

Leave Data Out with Pad Exposed
-------------------------------------------------------------------------
DHT11 Air Temp and Humidity
---------------------------
--> PIN 5
4.7K Pull Up Data to VCC

-------------------------------------------------------------------------
DS18 Ice Temperature
--------------------
--> PIN 6
4.7K Pull Up Data to VCC

-------------------------------------------------------------------------
TOGGLE Switches (4 Pin)
-----------------------



bossac -p COMx -e -w -v -R --offset=0x2000 adafruit-circuitpython-boardname-version.bin
C:\Users\cjcar\AppData\Local\Arduino15\packages\arduino\tools\bossac\1.7.0-arduino3>bossac.exe -e -w -v -R G:\GIT\IceReader2-client\build\IceReader2-client.ino.bin

Sketch uses 53180 bytes (20%) of program storage space. Maximum is 262144 bytes.
Waiting for upload port...
No upload port found, using COM10 as fallback
"C:\Users\cjcar\AppData\Local\Arduino15\packages\adafruit\tools\bossac\1.8.0-48-gb176eee/bossac.exe" -i -d --port=COM10 -U -i --offset=0x2000 -w -v "g:\GIT\IceReader2-client\build/IceReader2-client.ino.bin" -R
No device found on COM10
Error during Upload: Failed uploading: uploading error: exit status 1


Sketch uses 53180 bytes (20%) of program storage space. Maximum is 262144 bytes.
Waiting for upload port...
No upload port found, using COM10 as fallback
"C:\Users\cjcar\AppData\Local\Arduino15\packages\adafruit\tools\bossac\1.8.0-48-gb176eee/bossac.exe" -i -d --port=COM10 -U -i --offset=0x2000 -w -v "g:\GIT\IceReader2-client\build/IceReader2-client.ino.bin" -R
No device found on COM10
Error during Upload: Failed uploading: uploading error: exit status 1

*/

#include <Arduino.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <ArduinoLowPower.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

float checkBattery();
void powerDown();
float readIceTemp();
float readAirTemp();
float readAirHumidity();

#define VERSION 1.0
#define DEBUG 0

// Define Communication Pins
#define RFM95_CS    8
#define RFM95_INT   3
#define RFM95_RST   4

#define VBATPIN A7
#define airThermometerPin 5
#define iceThermometerPin 6

// Define Frequency for LoRa Radio
#define RF95_FREQ 433.0

// Define LoRa Transmit Power
// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

// The default transmitter power is 13dBm, using PA_BOOST.
// If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
// you can set transmitter powers from 5 to 23 dBm:
#define LORA_TRANSMIT_POWER 23

// Define Read Interval in ms
// 10 Seconds = 10000
// 15 Seconds = 15000
// 30 Seconds = 30000
// 1 Minute = 60000
// 5 Minutes = 300000
// 10 Minutes = 600000
// 30 Minutes = 1800000
// 60 Minutes = 3600000
#define READ_INTERVAL 600000

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

DHT dht(airThermometerPin, DHT22);
OneWire oneWire(iceThermometerPin);
DallasTemperature sensors(&oneWire);

// int on samd21 = Up to 2 Billion 2,147,483,647
// We don't go above 999,999
// At one read per minute, this gives us close to 2 years uptime without rollover.
int packetnum = 1;
float packetVersion = 1.1;

//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//

void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  pinMode(airThermometerPin, INPUT);
  pinMode(iceThermometerPin, INPUT);

  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) ;;
    delay(100);

    Serial.print("[INFO] StartUp - IceReader2 Client v"); Serial.println(VERSION);
    Serial.print("[INFO] StartUp - Packet Version v"); Serial.println(packetVersion);
    Serial.print("[INFO] StartUp - Resetting LoRa Radio... ");
  }

  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (DEBUG) { Serial.println("Done!"); }

  while (!rf95.init()) {
    if (DEBUG) { Serial.println("[ERROR] StartUp - LoRa radio initalication failed"); }
    while (1);
  }
  if (DEBUG) { Serial.println("[INFO] StartUp - LoRa radio initalication OK!"); }

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    if (DEBUG) { Serial.println("[ERROR] StartUp - LoRa setFrequency failed"); }
    while (1);
  }
  if (DEBUG) { Serial.print("[INFO] StartUp - LoRa Set Freq to: "); Serial.println(RF95_FREQ); }

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
  rf95.setTxPower(LORA_TRANSMIT_POWER, false);
  if (DEBUG) { Serial.print("[INFO] StartUp - LoRa Set TX Power to: "); Serial.println(LORA_TRANSMIT_POWER); }

  // Ice Thermometer Startup
  sensors.begin();
  if (DEBUG) { Serial.println("[INFO] StartUp - Ice Sensors Initialized"); }

  // Air Thermometer Startup
  dht.begin();
  if (DEBUG) { Serial.println("[INFO] StartUp - Air Sensors Initialized"); }
}

//---------------------------------------------------------------------------//

float checkBattery() {
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024;  // convert to voltage
  return(measuredvbat);
}

//---------------------------------------------------------------------------//

void powerDown() {

  if (DEBUG) { Serial.println("[INFO] - Powering Down Microcontroller"); }
  digitalWrite(13, LOW);
  LowPower.deepSleep(READ_INTERVAL);
}

//---------------------------------------------------------------------------//

float readIceTemp() {
  sensors.requestTemperatures();
  return sensors.getTempFByIndex(0);
}

float readAirTemp() {
  return dht.readTemperature(true);
}

float readAirHumidity() {
  return dht.readHumidity();
}

//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//

void loop() {

  //if (DEBUG) {
  //  USBDevice.init();
  //  USBDevice.attach();
  //}

  // Build datagram
  //SourceID:PacketVersion:PacketNum:AirTemp:AirHumidity:IceTemp
  //000:0.0:000000:00.0:00.0:00.0
  //1234567890123456789012345
  rf95.setModeTx();
  digitalWrite(13, HIGH);

  delay(1000); // Wait 1 second between transmits, could also 'sleep' here!

  if (DEBUG) { Serial.println("[INFO] -------> Starting Transmission <-------"); }

  // Note: Size 24. 23 Usable. End of Array is '\0'
  char radiopacket[33] = "0:0.0:000000:00.0:00.0:00.0:0.00";

  int myID = 3;
  float iceTempF = readIceTemp();
  float airTempF = readAirTemp();
  float humidity = readAirHumidity();
  float battery = checkBattery();

  if (DEBUG) {
    Serial.print("[INFO] IceTemp: "); Serial.println(iceTempF);
    Serial.print("[INFO] AirTemp: "); Serial.println(airTempF);
    Serial.print("[INFO] Humidity: "); Serial.println(humidity);
    Serial.print("[INFO] Battery: "); Serial.println(battery);
  }

  sprintf(radiopacket, "%0d:%2.1f:%06d:%04.1f:%04.1f:%04.1f:%3.2f", myID, packetVersion, packetnum, iceTempF, airTempF, humidity, battery);

  if (DEBUG) { Serial.print("[INFO] - Message To Send: "); Serial.println(radiopacket); }

  if (DEBUG) { Serial.println("[INFO] - Sending..."); }
  delay(10);

// -- THIS SHOULD BE ALREADY DONE BY THE 'send'
//  while(!rf95.waitCAD()) {
//    if (DEBUG) { Serial.println("[WARN] - Channel Busy."); }
//    delay(20);
//  }

  rf95.send((uint8_t *)radiopacket, 33);

  if (DEBUG) { Serial.println("[INFO] - Waiting for Confirmation..."); }
  delay(10);
  if (!rf95.waitPacketSent()) {
      if (DEBUG) { Serial.println("[ERROR] - Sending of message UNSUCCESSFUL"); }
  } else {
    packetnum++;
    if (DEBUG) { Serial.println("[INFO] - Sending of message SUCCESSFUL"); }
  }

  // Turn off the Radio
  if (DEBUG) { Serial.println("[INFO] - Powering Down Radio"); }
  rf95.sleep();

  digitalWrite(13, LOW);
  delay(12000); // Pause for 12 seconds before powering down.
  if (DEBUG) {
    delay(READ_INTERVAL);
  } else {
    powerDown();
  }
}


/*

  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

if (DEBUG) { Serial.println("Waiting for reply..."); }
  if (rf95.waitAvailableTimeout(1000)) {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      if (DEBUG) {
        Serial.print("Got reply: ");
        Serial.println((char*)buf);
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);
      }
    } else {
      if (DEBUG) { Serial.println("Receive failed"); }
    }
  } else {
    if (DEBUG) { Serial.println("No reply, is there a listener around?"); }
  }




*/

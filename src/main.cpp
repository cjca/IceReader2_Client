/*
IceReader2 Client - Copyright 2023, Curlvation, LLC
All Rights Reserved

Coretex-M (m0) 32-bit processor

 Pin Definitions
+---------------+

All Pin Labels are GPIO Numbers
Pin 0 = RX (Serial1 Input)
Pin 1 = TX (Serial1 Output)
Pin 20 = I2C SDA (Pull Up with 5K Resistor)
Pin 21 = I2C SCL (Pull Up with 5K Resistor)

Pin 5 = GPIO [DHT11 / Air Temp & Humidity]
Pin 6 = GPIO [DS18 / Ice Temp]

Pin 9 = A7 for Battery Voltage Resistor Divider Circuit

Pin 10 = NeoPixel

Pin 11 = GPIO
Pin 12 = GPIO

Pin 13 = Red LED @ USB Port

A0/14 = IO True Analog Output

A1/15 = Analog GPIO = Switch 0
A2/16 = Analog GPIO = Switch 1
A3/17 = Analog GPIO = Switch 2
A4/18 = Analog GPIO = Switch 3
A5/19 = Analog GPIO

Pin 24 = SPI SCK
Pin 23 = SPI MOSI
Pin 22 = SPI MISO

-------------------------------------------------------------------------
NeoPixel
--------
--> PIN 10
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
#include <main.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <RTCZero.h>
#include <ArduinoLowPower.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>


// Define Communication Pins
#define RFM95_CS    8
#define RFM95_INT   3
#define RFM95_RST   4

#define VBATPIN A7
#define airThermometerPin 5
#define iceThermometerPin 6
#define PIXEL_PIN 10

#define CONFIG_SWITCH_1 A1  // LSB StationID
#define CONFIG_SWITCH_2 A2  // MSB StationID
#define CONFIG_SWITCH_3 A3  // GroupID
#define CONFIG_SWITCH_4 A4  // Debug Mode

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

Adafruit_NeoPixel pixel(1, PIXEL_PIN, NEO_RGB + NEO_KHZ800);

// int on samd21 = Up to 2 Billion 2,147,483,647
// We don't go above 999,999
// At one read per minute, this gives us close to 2 years uptime without rollover.
uint16_t packetnum = 1;
const uint8_t packetVersion = 2;
uint8_t station_id = 0;
// const uint8_t myID = 3;

//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//

void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  pinMode(airThermometerPin, INPUT);
  pinMode(iceThermometerPin, INPUT);

  pinMode(CONFIG_SWITCH_1, INPUT_PULLUP);
  pinMode(CONFIG_SWITCH_2, INPUT_PULLUP);
  pinMode(CONFIG_SWITCH_3, INPUT_PULLUP);
  pinMode(CONFIG_SWITCH_4, INPUT_PULLUP);

  bool config_1 = digitalRead(CONFIG_SWITCH_1);
  bool config_2 = digitalRead(CONFIG_SWITCH_2);
  bool config_3 = digitalRead(CONFIG_SWITCH_3);
  bool config_4 = digitalRead(CONFIG_SWITCH_4);


  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) ;;
    delay(100);

    Serial.print("[INFO] StartUp - IceReader2 Client v"); Serial.println(VERSION);
    Serial.print("[INFO] StartUp - Packet Version v"); Serial.println(packetVersion);
    Serial.println("[INFO] StartUp - Switch Configurations");
    Serial.print("  [1]: "); Serial.println(config_1);
    Serial.print("  [2]: "); Serial.println(config_2);
    Serial.print("  [3]: "); Serial.println(config_3);
    Serial.print("  [4]: "); Serial.println(config_4);
    Serial.print("[INFO] StartUp - Resetting LoRa Radio... ");
  }

  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (DEBUG) { Serial.println("Done!"); }

  pixel.begin();

  while (!rf95.init()) {
    if (DEBUG) { Serial.println("[ERROR] StartUp - LoRa radio initalication failed"); }
    pixel.setPixelColor(0, 255, 0, 0);
    pixel.show();
    for (;;) {}
  }
  if (DEBUG) { Serial.println("[INFO] StartUp - LoRa radio initalication OK!"); }

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    if (DEBUG) { Serial.println("[ERROR] StartUp - LoRa setFrequency failed"); }
    pixel.setPixelColor(0, 255, 0, 0);
    pixel.show();
    for (;;) {}
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

  rf95.setModeTx();

  // Define Station from Config Bits
  // station_id = (config_1)+(config_2*2);
  station_id = (config_1)+(config_2*2)+1;

  int groupID = config_3;
  bool debug = config_4;

  if (DEBUG) {
    Serial.print("[INFO] Station ID: ");
    Serial.println(station_id);
    Serial.print("[INFO] Group ID: ");
    Serial.println(groupID);
    Serial.print("[INFO] debug ID: ");
    Serial.println(debug);
  }

  pixel.setPixelColor(0, 0, 255, 0);
  pixel.show();
  delay(1000);
}

//---------------------------------------------------------------------------//

uint16_t checkBattery() {
  float measuredvbat = analogRead(VBATPIN);
  if (DEBUG) {
    Serial.print("[INFO] AnalogRead VBAT: ");
    Serial.println(measuredvbat);
  }
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024.0;  // convert to voltage

  if (DEBUG) {
    Serial.print("[INFO] Calculated VBAT: ");
    Serial.println(measuredvbat);
  }

  return static_cast<uint16_t>(measuredvbat*100);
}

//---------------------------------------------------------------------------//

void powerDown() {
  if (DEBUG) { Serial.println("[INFO] - Powering Down Microcontroller"); }
  digitalWrite(13, LOW);
  LowPower.deepSleep(READ_INTERVAL);
  //  LowPower.sleep(READ_INTERVAL);
}

//---------------------------------------------------------------------------//

uint16_t readIceTemp() {
  sensors.requestTemperatures();
  float value = sensors.getTempFByIndex(0);
  if (DEBUG) {
    Serial.print("[INFO] Ice Temp Value Received: ");
    Serial.println(value);
  }
  if (value < 0) {
    return 0;
  } else {
    return static_cast<uint16_t>(value*100);
  }
}

uint16_t readAirTemp() {
  float value = dht.readTemperature(true);
  if (DEBUG) {
    Serial.print("[INFO] Air Temp Value Received: ");
    Serial.println(value);
  }
  if (isnan(value)) {
    return 0;
  } else {
    return static_cast<uint16_t>(value*100);
  }
}

uint16_t readAirHumidity() {
  float value = dht.readHumidity();
  if (DEBUG) {
    Serial.print("INFO] Humidity Value Received: ");
    Serial.println(value);
  }
  if (isnan(value)) {
    return 0;
  } else {
    return static_cast<uint16_t>(value*100);
  }
}

//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//

void loop() {
  // Build datagram
  // SourceID:PacketVersion:PacketNum:IceTemp:AirTemp:AirHumidity:battery
  // 00:00:000000:0000:0000:0000:000
  // 12345678901234567890123456789012

  digitalWrite(13, HIGH);

  delay(1000);  // Wait 1 second between transmits, could also 'sleep' here!
  if (DEBUG) { Serial.println("[INFO] ---> Starting Gather & Transmission <---"); }

  uint16_t iceTempF = readIceTemp();
  uint16_t airTempF = readAirTemp();
  uint16_t humidity = readAirHumidity();
  uint16_t battery = checkBattery();

  if (DEBUG) {
    Serial.print("[INFO] station_id: "); Serial.println(station_id);
    Serial.print("[INFO] packetVersion: "); Serial.println(packetVersion);
    Serial.print("[INFO] packetNum: "); Serial.println(packetnum);
    Serial.print("[INFO] IceTemp: "); Serial.println(iceTempF);
    Serial.print("[INFO] AirTemp: "); Serial.println(airTempF);
    Serial.print("[INFO] Humidity: "); Serial.println(humidity);
    Serial.print("[INFO] Battery: "); Serial.println(battery);
  }

  // Note: Size 32. 31 Usable. End of Array is '\0'
  char radiopacket[32] = "00:00:000000:0000:0000:0000:000";
  // char radiopacket[32];

  snprintf(radiopacket, sizeof(radiopacket),
   "%02u:%02u:%06u:%04u:%04u:%04u:%03u",
   station_id, packetVersion, packetnum, iceTempF, airTempF, humidity, battery);

  // snprintf(radiopacket, sizeof(radiopacket),
  //  "%0d:%2.1f:%06d:%04.1f:%04.1f:%04.1f:%3.2f",
  //  station_id, packetVersion, packetnum, iceTempF, airTempF, humidity, battery);

  if (DEBUG) { Serial.print("[INFO] - Message To Send: "); Serial.println(radiopacket); }

  if (DEBUG) { Serial.println("[INFO] - Sending..."); }
  delay(10);

//  rf95.send((uint8_t *)radiopacket, 33);
//  rf95.send((uint8_t *)radiopacket, sizeof(radiopacket));

  rf95.send(reinterpret_cast<uint8_t *>(radiopacket), sizeof(radiopacket));

  if (DEBUG) { Serial.println("[INFO] - Waiting for Confirmation..."); }
  delay(10);
  if (!rf95.waitPacketSent()) {
      if (DEBUG) { Serial.println("[ERROR] - Sending of message UNSUCCESSFUL"); }
  } else {
    // Our Packet number is included in the sent packet.  We only leave room
    // for 6 numbers.  This resets it to keep the packet from overflowing.
    if (packetnum >= 999999) {
      packetnum = 1;
    } else {
      packetnum++;
    }
    if (DEBUG) { Serial.println("[INFO] - Sending of message SUCCESSFUL"); }
    pixel.setPixelColor(0, 0, 0, 255);
    pixel.show();
    delay(3000);
  }

  // Turn off the Radio
  if (DEBUG) { Serial.println("[INFO] - Powering Down Radio"); }
  rf95.sleep();

  digitalWrite(13, LOW);
  pixel.clear();
  pixel.show();
  delay(12000);  // Pause for 12 seconds before powering down.
  if (DEBUG) {
    delay(READ_INTERVAL);
  } else {
    powerDown();
  }
}

/*
IceReader2 Client - Copyright 2023, Curlvation, LLC
All Rights Reserved
*/

#ifndef SRC_MAIN_H_
#define SRC_MAIN_H_

#define VERSION 1.0
#define DEBUG 0

float checkBattery();
void powerDown();
float readIceTemp();
float readAirTemp();
float readAirHumidity();

#endif  // SRC_MAIN_H_

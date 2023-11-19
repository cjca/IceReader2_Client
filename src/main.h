/*
IceReader2 Client - Copyright 2023, Curlvation, LLC
All Rights Reserved
*/

#ifndef SRC_MAIN_H_
#define SRC_MAIN_H_

#define VERSION 1.1
#define DEBUG 0

uint16_t checkBattery();
void powerDown();
uint16_t readIceTemp();
uint16_t readAirTemp();
uint16_t readAirHumidity();

#endif  // SRC_MAIN_H_

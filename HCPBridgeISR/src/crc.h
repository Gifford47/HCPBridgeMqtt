#ifndef __crc_h
#define __crc_h

#ifdef __cplusplus
extern "C" {
#endif

#include <Arduino.h>

//modbus crc calculation borrowed from:
//https://github.com/yaacov/ArduinoModbusSlave
#define MODBUS_CRC_LENGTH 2
#define readUInt16(arr, index) (arr[index]<<8 | arr[index + 1])
#define readCRC(arr, length) (arr[(length - MODBUS_CRC_LENGTH) + 1] << 8 | arr[length - MODBUS_CRC_LENGTH])

uint16_t calculateCRC(uint8_t *buffer, int length);

#ifdef __cplusplus
}
#endif

#endif
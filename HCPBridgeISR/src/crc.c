#include "crc.h"

/**
 * Calculate the CRC of the passed byte array from zero up to the passed length.
 *
 * @param buffer The byte array containing the data.
 * @param length The length of the byte array.
 *
 * @return The calculated CRC as an unsigned 16 bit integer.
 * 
 * Calculate and add the CRC.
 *       uint16_t crc = Modbus::calculateCRC(_responseBuffer, _responseBufferLength - MODBUS_CRC_LENGTH);
 *       _responseBuffer[_responseBufferLength - MODBUS_CRC_LENGTH] = crc & 0xFF;
 *       _responseBuffer[(_responseBufferLength - MODBUS_CRC_LENGTH) + 1] = crc >> 8;
 * 
 * 
 * #define MODBUS_FRAME_SIZE 4
 * #define MODBUS_CRC_LENGTH 2
 * uint16_t crc = readCRC(_requestBuffer, _requestBufferLength);
 * #define readUInt16(arr, index) word(arr[index], arr[index + 1])
 * #define readCRC(arr, length) word(arr[(length - MODBUS_CRC_LENGTH) + 1], arr[length - MODBUS_CRC_LENGTH])
 */
uint16_t ICACHE_RAM_ATTR calculateCRC(uint8_t *buffer, int length)
{
    int i, j;
    uint16_t crc = 0xFFFF;
    uint16_t tmp;

    // Calculate the CRC.
    for (i = 0; i < length; i++)
    {
        crc = crc ^ buffer[i];
        for (j = 0; j < 8; j++)
        {
            tmp = crc & 0x0001;
            crc = crc >> 1;
            if (tmp)
            {
                crc = crc ^ 0xA001;
            }
        }
    }
    return crc;
}

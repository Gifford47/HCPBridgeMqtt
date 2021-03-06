#ifndef __i2cshare_h
#define __i2cshare_h

#include <Arduino.h>

#define I2CADDR 7

#define I2C_CMD_CLOSEDOOR       0
#define I2C_CMD_OPENDOOR        1
#define I2C_CMD_OPENDOORHALF    4
#define I2C_CMD_STOPDOOR        2
#define I2C_CMD_VENTPOS         3
#define I2C_CMD_TOGGLELAMP      5

struct SHCIState{
    bool valid  : 1;
    bool lampOn : 1;
    uint8_t doorState; // see DoorState
    uint8_t doorCurrentPosition;
    uint8_t doorTargetPosition;
    uint8_t reserved;
    uint8_t cc;
};


#endif //__i2cshare_h
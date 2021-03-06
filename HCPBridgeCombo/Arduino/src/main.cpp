#include <Arduino.h>
#include <Wire.h>
#include "hciemulator.h"
#include "i2cshare.h"

#define RS485 Serial

// Hörmann HCP2 based on modbus rtu @57.6kB 8E1
HCIEmulator emulator(&RS485);


// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent() {  
  Wire.write((unsigned char*)&(emulator.getState()), sizeof(SHCIState));  
}

// function that executes whenever data is available by master
// this function is registered as an event, see setup()
void receiveEvent(int numBytes) {
  while (Wire.available()){
    switch(Wire.read()){
      case I2C_CMD_CLOSEDOOR:
        emulator.closeDoor();
        break;
      case I2C_CMD_OPENDOOR:
        emulator.openDoor();
        break;
      case I2C_CMD_OPENDOORHALF:
        emulator.openDoorHalf();
        break;
      case I2C_CMD_STOPDOOR:
        emulator.stopDoor();
        break;
      case I2C_CMD_VENTPOS:
        emulator.ventilationPosition();
        break;
      case I2C_CMD_TOGGLELAMP:
        emulator.toggleLamp();
        break;
    }
  }
}


// setup mcu
void setup(){
  
  //setup modbus
  RS485.begin(57600,SERIAL_8E1);

  //setup I2C
  Wire.begin(I2CADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
}

// mainloop
void loop(){     
  emulator.poll();
}
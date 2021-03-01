/*
 * ISR Code to emulate the modbus rtu protocol for hoermann HCP2
 * 
 * code based on https://github.com/juhovh/esp-uart
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Juho Vähä-Herttua (juhovh@iki.fi)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hciemulator.h"
#include "crc.h"

#include "osapi.h"
#include "ets_sys.h"
#include "uart_register.h"


#define T3_5  4800 //1750  
#define DEVICEID 0x02
#define BROADCASTID 0x00
#define SIMULATEKEYPRESSDELAYMS 100 
#define CHECKCHANGEDSET(Target,Value,Flag) if((Target)!=(Value)){Target=Value;Flag=true;}

#define UART0 0

// Missing defines from SDK
#define FUNC_U0RXD 0

// Define FIFO sizes, actually 128 but playing safe
#define UART_RXFIFO_SIZE 126
#define UART_TXFIFO_SIZE 126
#define UART_RXTOUT_TH   2
#define UART_RXFIFO_TH   100
#define UART_TXFIFO_TH   10

// Define some helper macros to handle FIFO functions
#define UART_TXFIFO_LEN(uart_no) \
  ((READ_PERI_REG(UART_STATUS(uart_no)) >> UART_TXFIFO_CNT_S) & UART_RXFIFO_CNT)
#define UART_TXFIFO_PUT(uart_no, byte) \
  WRITE_PERI_REG(UART_FIFO(uart_no), (byte) & 0xff)
#define UART_RXFIFO_LEN(uart_no) \
  ((READ_PERI_REG(UART_STATUS(uart_no)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT)
#define UART_RXFIFO_GET(uart_no) \
  READ_PERI_REG(UART_FIFO(uart_no))



bool skipFrame = false;

int recvTime = 0;
int lastSendTime = 0;
int lastStateTime = 0;
size_t rxlen = 0;
size_t txlen = 0;
size_t txpos = 0;
unsigned char rxbuffer[255];
unsigned char txbuffer[255];
EStateMachine statemachine = WAITING;
SHCIState state;
//--------------------------------------------------------------

void openDoor(){
    if(statemachine != WAITING){
        return;
    }
    lastStateTime = millis();
    statemachine = STARTOPENDOOR;
}

void openDoorHalf(){
    if(statemachine != WAITING){
        return;
    }
    lastStateTime = millis();
    statemachine = STARTOPENDOORHALF;
}

void closeDoor(){
    if(statemachine != WAITING){
        return;
    }
    lastStateTime = millis();    
    statemachine = STARTCLOSEDOOR;
}

void stopDoor(){
    if(statemachine != WAITING){
        return;
    }
    lastStateTime = millis();    
    statemachine = STARTSTOPDOOR;
}

void toggleLamp(){
    if(statemachine != WAITING){
        return;
    }
    lastStateTime = millis();
    statemachine = STARTTOGGLELAMP;
}

void ventilationPosition(){
    if(statemachine != WAITING){
        return;
    }
    lastStateTime = millis();
    statemachine = STARTVENTPOSITION;
}

unsigned long getMessageAge(){
  return micros()-recvTime;
}

SHCIState* getHCIState(){
  return &state;
}


//--------------------------------------------------------------

const unsigned char ResponseTemplate_Fcn17_Cmd03_L08 []= {0x02,0x17,0x10,0x3E,0x00,0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x74,0x1B};
const unsigned char ResponseTemplate_Fcn17_Cmd04_L02 []= {0x02,0x17,0x04,0x0F,0x00,0x04,0xFD,0x0A,0x72};
static void ICACHE_RAM_ATTR processDeviceStatusFrame(){
    if(rxlen==0x11){  
        unsigned char counter = rxbuffer[11];
        unsigned char cmd = rxbuffer[12];
        if(rxbuffer[5] == 0x08){
                // expose internal state
            //      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
            //0011: 02 17 9C B9 00 08 9C 41 00 02 04 3E 03 00 00 EB CC
            //res=> 02 17 10 3E 00 03 01 00 00 00 00 00 00 00 00 00 00 00 00 74 1B                 
            memcpy_P(txbuffer, ResponseTemplate_Fcn17_Cmd03_L08, sizeof(ResponseTemplate_Fcn17_Cmd03_L08));  
            txbuffer[0] = rxbuffer[0];
            txbuffer[3] = counter;
            txbuffer[5] = cmd;
            txlen = sizeof(ResponseTemplate_Fcn17_Cmd03_L08);   

            
            switch(statemachine)
            {
                // open Door
                case STARTOPENDOOR:
                    txbuffer[7]= 0x02;
                    txbuffer[8]= 0x10;
                    statemachine = STARTOPENDOOR_RELEASE;
                    lastStateTime = millis();
                    break;
                case STARTOPENDOOR_RELEASE:
                    if(lastStateTime+SIMULATEKEYPRESSDELAYMS<millis()){
                        txbuffer[7]= 0x01;
                        txbuffer[8]= 0x10;
                        statemachine = WAITING; 
                    }
                    break;

                // close Door
                case STARTCLOSEDOOR:
                    txbuffer[7]= 0x02;
                    txbuffer[8]= 0x20;
                    statemachine = STARTCLOSEDOOR_RELEASE;
                    lastStateTime = millis();
                    break;
                case STARTCLOSEDOOR_RELEASE:
                    if(lastStateTime+SIMULATEKEYPRESSDELAYMS<millis()){
                        txbuffer[7]= 0x01;
                        txbuffer[8]= 0x20;
                        statemachine = WAITING; 
                    }
                    break;   

                // stop Door
                case STARTSTOPDOOR:
                    txbuffer[7]= 0x02;
                    txbuffer[8]= 0x40;
                    statemachine = STARTSTOPDOOR_RELEASE;
                    lastStateTime = millis();
                    break;
                case STARTSTOPDOOR_RELEASE:
                    if(lastStateTime+SIMULATEKEYPRESSDELAYMS<millis()){
                        txbuffer[7]= 0x01;
                        txbuffer[8]= 0x40;
                        statemachine = WAITING; 
                    }
                    break;

                // Ventilation
                case STARTVENTPOSITION:
                    txbuffer[7]= 0x02;
                    txbuffer[9]= 0x40;
                    statemachine = STARTVENTPOSITION_RELEASE;
                    lastStateTime = millis();
                    break;
                case STARTVENTPOSITION_RELEASE:
                    if(lastStateTime+SIMULATEKEYPRESSDELAYMS<millis()){
                        txbuffer[7]= 0x01;
                        txbuffer[9]= 0x40;
                        statemachine = WAITING; 
                    }
                    break;        


                // Half Position
                case STARTOPENDOORHALF:
                    txbuffer[7]= 0x02;
                    txbuffer[9]= 0x04;
                    statemachine = STARTOPENDOORHALF_RELEASE;
                    lastStateTime = millis();
                    break;

                case STARTOPENDOORHALF_RELEASE:
                    if(lastStateTime+SIMULATEKEYPRESSDELAYMS<millis()){
                        txbuffer[7]= 0x01;
                        txbuffer[9]= 0x04;
                        statemachine = WAITING; 
                    }
                    break;                                          

                // Toggle Lamp
                case STARTTOGGLELAMP:
                    txbuffer[7]= 0x10;
                    txbuffer[9]= 0x02;
                    statemachine = STARTTOGGLELAMP_RELEASE;
                    lastStateTime = millis();
                    break;
                case STARTTOGGLELAMP_RELEASE:
                    if(lastStateTime+SIMULATEKEYPRESSDELAYMS<millis()){
                        txbuffer[7]= 0x08;
                        txbuffer[9]= 0x02;
                        statemachine = WAITING; 
                    }
                    break;  
                    
                case WAITING:
                    break;            
            }                                 
            return;             
        }
        else if(rxbuffer[5] == 0x02){
            //      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
            //0011: 02 17 9C B9 00 02 9C 41 00 02 04 0F 04 17 00 7B 21
            //res=> 02 17 04 0F 00 04 FD 0A 72     
            memcpy_P(txbuffer, ResponseTemplate_Fcn17_Cmd04_L02, sizeof(ResponseTemplate_Fcn17_Cmd04_L02));  
            txbuffer[0] = rxbuffer[0];
            txbuffer[3] = counter;
            txbuffer[5] = cmd;
            txlen = sizeof(ResponseTemplate_Fcn17_Cmd04_L02);   
            return;
        }
    }
    //Log3(LL_ERROR,"Frame skipped, unexpected data: ", m_rxbuffer, m_rxlen); 
}

const unsigned char ResponseTemplate_Fcn17_Cmd02_L05 []= {0x02,0x17,0x0a,0x00,0x00,0x02,0x05,0x04,0x30,0x10,0xff,0xa8,0x45,0x0e,0xdf};
static void ICACHE_RAM_ATTR processDeviceBusScanFrame(){
    //      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
    //0013: 02 17 9C B9 00 05 9C 41 00 03 06 00 02 00 00 01 02 f8 35
    //res=> 02 17 0a 00 00 02 05 04 30 10 ff a8 45 0e df
    unsigned char counter = rxbuffer[11];
    unsigned char cmd = rxbuffer[12];
    memcpy_P(txbuffer, ResponseTemplate_Fcn17_Cmd02_L05, sizeof(ResponseTemplate_Fcn17_Cmd02_L05));  
    txbuffer[0] = rxbuffer[0];
    txbuffer[3] = counter;
    txbuffer[5] = cmd;
    txlen = sizeof(ResponseTemplate_Fcn17_Cmd02_L05); 

    //Log(LL_INFO,"Busscan received");  
}

static void ICACHE_RAM_ATTR processBroadcastStatusFrame(){
    //001B: 00 10 9D 31 00 09 12 64 00 00 00 40 60 00 00 00 00 00 00 00 00 00 01 00 00 CA 22    
    bool hasStateChanged = false;
    CHECKCHANGEDSET(state.lampOn,rxbuffer[20] == 0x14,hasStateChanged);      
    CHECKCHANGEDSET(state.doorCurrentPosition,rxbuffer[10],hasStateChanged);
    CHECKCHANGEDSET(state.doorTargetPosition, rxbuffer[9],hasStateChanged);
    CHECKCHANGEDSET(state.doorState, rxbuffer[11],hasStateChanged);
    CHECKCHANGEDSET(state.reserved, rxbuffer[17],hasStateChanged);
    CHECKCHANGEDSET(state.valid, true,hasStateChanged);                       
}    

static bool ICACHE_RAM_ATTR processFrame(){    
    switch (rxlen)
    {
      case 0x1b:
      case 0x11:
      case 0x13:      
      break;
    
    default:
      // unexpected rxlen
      return false;
    }

    txlen=txpos=0;

    // check device id, pass only device id 2 and 0 (broadcast)
    if(rxbuffer[0] != BROADCASTID && rxbuffer[0] != DEVICEID){
        // Frame skipped, unsupported device id
        return false;        
    }

    // check crc
    uint16_t crc = readCRC(rxbuffer, rxlen);        
    if(crc != calculateCRC(rxbuffer,rxlen-MODBUS_CRC_LENGTH)){
        // Frame skipped, wrong crc 
        return false;
    }

    // dispatch modbus function
    switch(rxbuffer[1]){ 
        case 0x10:{  //  Write Multiple registers                            
            if(rxlen == 0x1b && rxbuffer[0] == BROADCASTID)
            {
                processBroadcastStatusFrame();
                return true;
            }  
            break;
        }
        
        case 0x17:{ // Read/Write Multiple registers
            if(rxbuffer[0] == DEVICEID){
                switch(rxlen){
                    case 0x11:{
                        processDeviceStatusFrame();
                        return true;
                    }

                    case 0x13:
                        processDeviceBusScanFrame();
                        return true;
                }
            }
            break;                               
        }
    }

    // Frame skipped, unexpected data
    return false;
}


static void ICACHE_RAM_ATTR processMessage (){    
    // check frame, process frame    
    if(statemachine!= WAITING && lastStateTime+2000<millis()){
        statemachine = WAITING;
    }
    
    if(!processFrame()){
        return;
    }

    rxlen = 0;

    // prepare response fix crc
    if(txlen > 0){                            
        uint16_t crc = calculateCRC(txbuffer, txlen - MODBUS_CRC_LENGTH);
        txbuffer[txlen - MODBUS_CRC_LENGTH] = crc & 0xFF;
        txbuffer[(txlen - MODBUS_CRC_LENGTH) + 1] = crc >> 8; 

        // Enable send      
        SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
    }                    
       
}

//--------------------------------------------------------------

static uint16 ICACHE_RAM_ATTR uart0_receive(){
  uint16 i;
  uint16 uart0_rxfifo_len = UART_RXFIFO_LEN(UART0);

  if(rxlen>0 && (micros()-recvTime > T3_5)){
      // last message timeout, reset buffer
      rxlen = 0;
      skipFrame = false;
  }

  // receive data from uart
  for (i=0; i<uart0_rxfifo_len; i++) {
    if (rxlen < sizeof(rxbuffer)) {
      rxbuffer[rxlen++] = UART_RXFIFO_GET(UART0);                
    }else{
      // buffer overflow!!!
      skipFrame = true;
      UART_RXFIFO_GET(UART0); 
    }    
    recvTime = micros();
  }

  if(!skipFrame && rxlen>0){
    processMessage();
  }
  
  return i;
}

static uint16 ICACHE_RAM_ATTR uart0_send(){
  uint16 i;
  for (i=UART_TXFIFO_LEN(UART0); i<UART_TXFIFO_SIZE; i++) {
      if (txpos==txlen) {
        txpos=txlen=0;
        CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
        break;
      }
      UART_TXFIFO_PUT(UART0, txbuffer[txpos++]);
      lastSendTime = micros()-recvTime;
  }  
  return i-1;
}


static void ICACHE_RAM_ATTR uart0_intr_handler(void *arg){
  uint32 uart0_status;
  uart0_status = READ_PERI_REG(UART_INT_ST(UART0));
  if (uart0_status & UART_RXFIFO_TOUT_INT_ST) {    
    // No character received for some time, read to ringbuf
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_ST);
    uart0_receive();    

  } else if (uart0_status & UART_RXFIFO_FULL_INT_ST) {    
    // RX buffer becoming full, 
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_ST);
    uart0_receive();    

  } else if (uart0_status & UART_TXFIFO_EMPTY_INT_ST) {    
    // TX buffer empty, check if ringbuf has space or disable int
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_TXFIFO_EMPTY_INT_ST);    
    uart0_send();
  }

}

void ICACHE_FLASH_ATTR
uart0_open(uint32 baud_rate, uint32 flags){
  uint32 clkdiv;

  ETS_UART_INTR_DISABLE();

  // Set both RX and TX pins to correct mode
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);

  // Disable pullup on TX pin, enable on RX pin
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
  PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);

  // Configure baud rate for the port
  clkdiv = (UART_CLK_FREQ / baud_rate) & UART_CLKDIV_CNT;
  WRITE_PERI_REG(UART_CLKDIV(UART0), clkdiv);

  // Configure parameters for the port
  WRITE_PERI_REG(UART_CONF0(UART0), flags);

  // Reset UART0
  uart0_reset(baud_rate, flags);
}

void ICACHE_FLASH_ATTR
uart0_reset(){
  // Disable interrupts while resetting UART0
  ETS_UART_INTR_DISABLE();

  // Clear all RX and TX buffers and flags
  SET_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST | UART_TXFIFO_RST);

  // Set RX and TX interrupt thresholds
  WRITE_PERI_REG(UART_CONF1(UART0),
    UART_RX_TOUT_EN |
    ((UART_RXTOUT_TH & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S) |
    ((UART_RXFIFO_TH & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
    ((UART_TXFIFO_TH & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S));

  // Disable all existing interrupts and enable ours
  WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);
  SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_RXFIFO_TOUT_INT_ENA);
  SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_RXFIFO_FULL_INT_ENA);

  // Restart the interrupt handler for UART0
  ETS_UART_INTR_ATTACH(uart0_intr_handler, NULL);
  ETS_UART_INTR_ENABLE();


  state.valid = false;
}

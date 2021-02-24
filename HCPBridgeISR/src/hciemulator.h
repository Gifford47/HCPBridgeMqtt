/* modified/reduced to provide HCP Modbus low level functions
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
#ifndef ESP_UART_H
#define ESP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <c_types.h>

typedef enum {
    WAITING,

    STARTOPENDOOR,
    STARTOPENDOOR_RELEASE,

    STARTOPENDOORHALF,
    STARTOPENDOORHALF_RELEASE,

    STARTCLOSEDOOR,
    STARTCLOSEDOOR_RELEASE,

    STARTSTOPDOOR,
    STARTSTOPDOOR_RELEASE,

    STARTTOGGLELAMP,
    STARTTOGGLELAMP_RELEASE,

    STARTVENTPOSITION,
    STARTVENTPOSITION_RELEASE
} EStateMachine;

typedef struct {
    int valid;
    int lampOn;
    uint8_t doorState; // see DoorState
    uint8_t doorCurrentPosition;
    uint8_t doorTargetPosition;
    uint8_t reserved;
} SHCIState;

SHCIState* getHCIState();
unsigned long getMessageAge();
void openDoor();
void openDoorHalf();
void closeDoor();
void stopDoor();
void toggleLamp();
void ventilationPosition();


#define UART_STOP_BITS_ONE      0x10
#define UART_STOP_BITS_ONE_HALF 0x20
#define UART_STOP_BITS_TWO      0x30
#define UART_STOP_BITS_MASK     0x30

#define UART_BITS_FIVE          0x00
#define UART_BITS_SIX           0x04
#define UART_BITS_SEVEN         0x08
#define UART_BITS_EIGHT         0x0C
#define UART_BITS_MASK          0x0C

#define UART_PARITY_NONE        0x00
#define UART_PARITY_EVEN        0x02
#define UART_PARITY_ODD         0x03
#define UART_PARITY_MASK        0x03

#define UART_FLAGS_8N1 (UART_BITS_EIGHT | UART_PARITY_NONE | UART_STOP_BITS_ONE)
#define UART_FLAGS_8E1 (UART_BITS_EIGHT | UART_PARITY_EVEN | UART_STOP_BITS_ONE)

#define UART_STATUS_IDLE        0x00
#define UART_STATUS_RECEIVING   0x01
#define UART_STATUS_OVERFLOW    0x02

void uart0_open(uint32 baud_rate, uint32 flags);
void uart0_reset();

uint16 uart0_available();
uint16 uart0_read_buf(const void *buf, uint16 nbyte, uint16 timeout);
uint16 uart0_write_buf(const void *buf, uint16 nbyte, uint16 timeout);
void uart0_flush();

void uart1_open(uint32 baud_rate, uint32 flags);
void uart1_reset();
uint8 uart1_write_byte(uint8 byte);

#ifdef __cplusplus
}
#endif

#endif

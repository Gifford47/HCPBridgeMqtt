# esp-uart

Blocking ESP8266 UART driver with interrupt-driven RX and TX buffers.

I needed an UART driver where I have complete control over the input and output, because I have the UART chip attached to a multiplexer that connects to multiple serial ports. The driver supplied by the manufacturer works by queueing read callback tasks from the interrupts, which was unnecessary complication for my use case. This driver is a bit work in progress, but should be already usable.

The license of uart_register.h is a little bit unclear, but because it only
defines the hardware memory locations, it should not be copyrightable. In any
case it can only be used on ESP8266 and is necessary to be able to use the UART
functionality, so the manufacturer most likely does not have any issues with using it.
